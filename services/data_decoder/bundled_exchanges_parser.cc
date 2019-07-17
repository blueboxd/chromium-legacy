// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/bundled_exchanges_parser.h"

#include <algorithm>

#include "base/big_endian.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/cbor/reader.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"

namespace data_decoder {

namespace {

// The maximum length of the CBOR item header (type and argument).
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#parse-type-argument
constexpr uint64_t kMaxCBORItemHeaderSize = 9;

// The maximum size of the section-lengths CBOR item.
constexpr uint64_t kMaxSectionLengthsCBORSize = 8192;

// The maximum size of the index section allowed in this implementation.
constexpr uint64_t kMaxIndexSectionSize = 1 * 1024 * 1024;

// The maximum size of the response header CBOR.
constexpr uint64_t kMaxResponseHeaderLength = 512 * 1024;

// The initial buffer size for reading an item from the response section.
constexpr uint64_t kInitialBufferSizeForResponse = 4096;

const uint8_t kBundleMagicBytes[] = {
    0x84, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6,
};

// Section names.
constexpr char kIndexSection[] = "index";
constexpr char kResponsesSection[] = "responses";

// https://tools.ietf.org/html/draft-ietf-cbor-7049bis-05#section-3.1
enum class CBORType {
  kByteString = 2,
  kArray = 4,
};

// A list of (section-name, length) pairs.
using SectionLengths = std::vector<std::pair<std::string, uint64_t>>;

// Parses a `section-lengths` CBOR item.
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
base::Optional<SectionLengths> ParseSectionLengths(
    base::span<const uint8_t> data) {
  cbor::Reader::DecoderError error;
  base::Optional<cbor::Value> value = cbor::Reader::Read(data, &error);
  if (!value.has_value() || !value->is_array())
    return base::nullopt;

  const cbor::Value::ArrayValue& array = value->GetArray();
  if (array.size() % 2 != 0)
    return base::nullopt;

  SectionLengths result;
  for (size_t i = 0; i < array.size(); i += 2) {
    if (!array[i].is_string() || !array[i + 1].is_unsigned())
      return base::nullopt;
    result.emplace_back(array[i].GetString(), array[i + 1].GetUnsigned());
  }
  return result;
}

struct ParsedHeaders {
  base::flat_map<std::string, std::string> headers;
  base::flat_map<std::string, std::string> pseudos;
};

// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#cbor-headers
base::Optional<ParsedHeaders> ConvertCBORValueToHeaders(
    const cbor::Value& headers_value) {
  // Step 1. If item doesn’t match the headers rule in the above CDDL, return
  // an error.
  if (!headers_value.is_map())
    return base::nullopt;

  // Step 2. Let headers be a new header list ([FETCH]).
  // Step 3. Let pseudos be an empty map ([INFRA]).
  ParsedHeaders result;

  // Step 4. For each pair (name, value) in item:
  for (const auto& item : headers_value.GetMap()) {
    if (!item.first.is_bytestring() || !item.second.is_bytestring())
      return base::nullopt;
    base::StringPiece name = item.first.GetBytestringAsString();
    base::StringPiece value = item.second.GetBytestringAsString();

    // Step 4.1. If name contains any upper-case or non-ASCII characters, return
    // an error. This matches the requirement in Section 8.1.2 of [RFC7540].
    if (!base::IsStringASCII(name) ||
        std::any_of(name.begin(), name.end(), base::IsAsciiUpper<char>))
      return base::nullopt;

    // Step 4.2. If name starts with a ‘:’:
    if (!name.empty() && name[0] == ':') {
      // Step 4.2.1. Assert: pseudos[name] does not exist, because CBOR maps
      // cannot contain duplicate keys.
      // This is ensured by cbor::Reader.
      DCHECK(!result.pseudos.contains(name));
      // Step 4.2.2. Set pseudos[name] to value.
      result.pseudos.insert(
          std::make_pair(name.as_string(), value.as_string()));
      // Step 4.3.3. Continue.
      continue;
    }

    // Step 4.3. If name or value doesn’t satisfy the requirements for a header
    // in [FETCH], return an error.
    if (!net::HttpUtil::IsValidHeaderName(name) ||
        !net::HttpUtil::IsValidHeaderValue(value))
      return base::nullopt;

    // Step 4.4. Assert: headers does not contain ([FETCH]) name, because CBOR
    // maps cannot contain duplicate keys and an earlier step rejected
    // upper-case bytes.
    // This is ensured by cbor::Reader.
    DCHECK(!result.headers.contains(name));

    // Step 4.5. Append (name, value) to headers.
    result.headers.insert(std::make_pair(name.as_string(), value.as_string()));
  }

  // Step 5. Return (headers, pseudos).
  return result;
}

// A utility class for reading various values from input buffer.
class InputReader {
 public:
  explicit InputReader(base::span<const uint8_t> buf) : buf_(buf) {}

  uint64_t CurrentOffset() const { return current_offset_; }
  size_t Size() const { return buf_.size(); }

  base::Optional<uint8_t> ReadByte() {
    if (buf_.empty())
      return base::nullopt;
    uint8_t byte = buf_[0];
    Advance(1);
    return byte;
  }

  template <typename T>
  bool ReadBigEndian(T* out) {
    auto bytes = ReadBytes(sizeof(T));
    if (!bytes)
      return false;
    base::ReadBigEndian(reinterpret_cast<const char*>(bytes->data()), out);
    return true;
  }

  base::Optional<base::span<const uint8_t>> ReadBytes(size_t n) {
    if (buf_.size() < n)
      return base::nullopt;
    auto result = buf_.subspan(0, n);
    Advance(n);
    return result;
  }

  // Parses the type and argument of a CBOR item from the input head. If parsed
  // successfully and the type matches |expected_type|, returns the argument.
  // Otherwise returns nullopt.
  base::Optional<uint64_t> ReadCBORHeader(CBORType expected_type) {
    auto pair = ReadTypeAndArgument();
    if (!pair || pair->first != expected_type)
      return base::nullopt;
    return pair->second;
  }

 private:
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#parse-type-argument
  base::Optional<std::pair<CBORType, uint64_t>> ReadTypeAndArgument() {
    base::Optional<uint8_t> first_byte = ReadByte();
    if (!first_byte)
      return base::nullopt;

    CBORType type = static_cast<CBORType>((*first_byte & 0xE0) / 0x20);
    uint8_t b = *first_byte & 0x1F;

    if (b <= 23)
      return std::make_pair(type, b);
    if (b == 24) {
      auto content = ReadByte();
      if (!content || *content < 24)
        return base::nullopt;
      return std::make_pair(type, *content);
    }
    if (b == 25) {
      uint16_t content;
      if (!ReadBigEndian(&content) || content >> 8 == 0)
        return base::nullopt;
      return std::make_pair(type, content);
    }
    if (b == 26) {
      uint32_t content;
      if (!ReadBigEndian(&content) || content >> 16 == 0)
        return base::nullopt;
      return std::make_pair(type, content);
    }
    if (b == 27) {
      uint64_t content;
      if (!ReadBigEndian(&content) || content >> 32 == 0)
        return base::nullopt;
      return std::make_pair(type, content);
    }
    return base::nullopt;
  }

  void Advance(size_t n) {
    DCHECK_LE(n, buf_.size());
    buf_ = buf_.subspan(n);
    current_offset_ += n;
  }

  base::span<const uint8_t> buf_;
  uint64_t current_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InputReader);
};

}  // namespace

class BundledExchangesParser::SharedBundleDataSource::Observer {
 public:
  Observer() {}
  virtual ~Observer() {}
  virtual void OnDisconnect() = 0;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// A parser for bundle's metadata. This currently looks only at the "index"
// section. This class owns itself and will self destruct after calling the
// ParseMetadataCallback.
// TODO(crbug.com/966753): Add support for the "manifest" section and "critical"
// section.
class BundledExchangesParser::MetadataParser
    : BundledExchangesParser::SharedBundleDataSource::Observer {
 public:
  MetadataParser(scoped_refptr<SharedBundleDataSource> data_source,
                 ParseMetadataCallback callback)
      : data_source_(data_source), callback_(std::move(callback)) {
    data_source_->AddObserver(this);
  }
  ~MetadataParser() override { data_source_->RemoveObserver(this); }

  void Start() {
    data_source_->GetSize(base::BindOnce(&MetadataParser::DidGetSize,
                                         weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetSize(uint64_t size) {
    size_ = size;

    // In the next step, we will parse `magic`, `section-lengths`, and the CBOR
    // header of `sections`.
    // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#top-level
    const uint64_t length =
        std::min(size, sizeof(kBundleMagicBytes) + kMaxSectionLengthsCBORSize +
                           kMaxCBORItemHeaderSize * 2);
    data_source_->Read(0, length,
                       base::BindOnce(&MetadataParser::ParseBundleHeader,
                                      weak_factory_.GetWeakPtr(), length));
  }

  // Step 1-15 of
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
  void ParseBundleHeader(uint64_t expected_data_length,
                         const base::Optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunCallbackAndDestroy(nullptr, "Error reading bundle header.");
      return;
    }

    // Step 1. "Seek to offset 0 in stream. Assert: this operation doesn't
    // fail."
    InputReader input(*data);

    // Step 2. "If reading 10 bytes from stream returns an error or doesn't
    // return the bytes with hex encoding "84 48 F0 9F 8C 90 F0 9F 93 A6"
    // (the CBOR encoding of the 4-item array initial byte and 8-byte bytestring
    // initial byte, followed by 🌐📦 in UTF-8), return an error."
    const auto magic = input.ReadBytes(sizeof(kBundleMagicBytes));
    if (!magic ||
        !std::equal(magic->begin(), magic->end(), std::begin(kBundleMagicBytes),
                    std::end(kBundleMagicBytes))) {
      RunCallbackAndDestroy(nullptr, "Wrong magic bytes.");
      return;
    }

    // Step 3. "Let sectionLengthsLength be the result of getting the length of
    // the CBOR bytestring header from stream (Section 3.5.2). If this is an
    // error, return that error."
    const auto section_lengths_length =
        input.ReadCBORHeader(CBORType::kByteString);
    if (!section_lengths_length) {
      RunCallbackAndDestroy(nullptr,
                            "Cannot parse the size of section-lengths.");
      return;
    }
    // Step 4. "If sectionLengthsLength is 8192 (8*1024) or greater, return an
    // error."
    if (*section_lengths_length >= kMaxSectionLengthsCBORSize) {
      RunCallbackAndDestroy(
          nullptr, "The section-lengths CBOR must be smaller than 8192 bytes.");
      return;
    }

    // Step 5. "Let sectionLengthsBytes be the result of reading
    // sectionLengthsLength bytes from stream. If sectionLengthsBytes is an
    // error, return that error."
    const auto section_lengths_bytes = input.ReadBytes(*section_lengths_length);
    if (!section_lengths_bytes) {
      RunCallbackAndDestroy(nullptr, "Cannot read section-lengths.");
      return;
    }

    // Step 6. "Let sectionLengths be the result of parsing one CBOR item
    // (Section 3.5) from sectionLengthsBytes, matching the section-lengths
    // rule in the CDDL ([I-D.ietf-cbor-cddl]) above. If sectionLengths is an
    // error, return an error."
    const auto section_lengths = ParseSectionLengths(*section_lengths_bytes);
    if (!section_lengths) {
      RunCallbackAndDestroy(nullptr, "Cannot parse section-lengths.");
      return;
    }

    // Step 7. "Let (sectionsType, numSections) be the result of parsing the
    // type and argument of a CBOR item from stream (Section 3.5.3)."
    const auto num_sections = input.ReadCBORHeader(CBORType::kArray);
    if (!num_sections) {
      RunCallbackAndDestroy(nullptr, "Cannot parse the number of sections.");
      return;
    }

    // Step 8. "If sectionsType is not 4 (a CBOR array) or numSections is not
    // half of the length of sectionLengths, return an error."
    if (*num_sections != section_lengths->size()) {
      RunCallbackAndDestroy(nullptr, "Unexpected number of sections.");
      return;
    }

    // Step 9. "Let sectionsStart be the current offset within stream."
    const uint64_t sections_start = input.CurrentOffset();

    // Step 10. "Let knownSections be the subset of the Section 6.2 that this
    // client has implemented."
    // Step 11. "Let ignoredSections be an empty set."

    // This implementation doesn't use knownSections nor ignoredSections.

    // Step 12. "Let sectionOffsets be an empty map ([INFRA]) from section names
    // to (offset, length) pairs. These offsets are relative to the start of
    // stream."

    // |section_offsets_| is defined as a class member field.

    // Step 13. "Let currentOffset be sectionsStart."
    uint64_t current_offset = sections_start;

    // Step 14. "For each ("name", length) pair of adjacent elements in
    // sectionLengths:"
    for (const auto& pair : *section_lengths) {
      const std::string& name = pair.first;
      const uint64_t length = pair.second;
      // Step 14.1. "If "name"'s specification in knownSections says not to
      // process other sections, add those sections' names to ignoredSections."

      // There're no such sections at the moment.

      // Step 14.2. "If sectionOffsets["name"] exists, return an error. That is,
      // duplicate sections are forbidden."
      // Step 14.3. "Set sectionOffsets["name"] to (currentOffset, length)."
      bool added = section_offsets_
                       .insert(std::make_pair(
                           name, std::make_pair(current_offset, length)))
                       .second;
      if (!added) {
        RunCallbackAndDestroy(nullptr, "Duplicated section.");
        return;
      }

      // Step 14.4. "Set currentOffset to currentOffset + length."
      if (!base::CheckAdd(current_offset, length)
               .AssignIfValid(&current_offset) ||
          current_offset > size_) {
        RunCallbackAndDestroy(nullptr, "Section doesn't fit in the bundle.");
        return;
      }
    }

    // Step 15. "If the "responses" section is not last in sectionLengths,
    // return an error. This allows a streaming parser to assume that it'll
    // know the requests by the time their responses arrive."
    if (section_lengths->empty() ||
        section_lengths->back().first != kResponsesSection) {
      RunCallbackAndDestroy(
          nullptr, "Responses section is not the last in section-lengths.");
      return;
    }

    // Read the index section.
    auto index_section = section_offsets_.find(kIndexSection);
    if (index_section == section_offsets_.end()) {
      RunCallbackAndDestroy(nullptr, "No index section.");
      return;
    }
    const uint64_t index_section_offset = index_section->second.first;
    const uint64_t index_section_length = index_section->second.second;

    if (index_section_length > kMaxIndexSectionSize) {
      RunCallbackAndDestroy(nullptr,
                            "Index section larger than 1MB is not supported.");
      return;
    }

    data_source_->Read(
        index_section_offset, index_section_length,
        base::BindOnce(&MetadataParser::ParseIndexSection,
                       weak_factory_.GetWeakPtr(), index_section_length));
  }

  // Step 1. of
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#index-section
  void ParseIndexSection(uint64_t expected_data_length,
                         const base::Optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunCallbackAndDestroy(nullptr, "Error reading index section.");
      return;
    }

    // Step 1. "Let index be the result of parsing sectionContents as a CBOR
    // item matching the index rule in the above CDDL (Section 3.5). If index is
    // an error, return an error."
    cbor::Reader::DecoderError error;
    base::Optional<cbor::Value> index_section_value =
        cbor::Reader::Read(*data, &error);
    if (!index_section_value) {
      VLOG(1) << cbor::Reader::ErrorCodeToString(error);
      RunCallbackAndDestroy(nullptr, "Error parsing index section.");
      return;
    }
    if (!index_section_value->is_array()) {
      RunCallbackAndDestroy(nullptr, "Index section must be an array.");
      return;
    }

    // Read the header of the response section.
    auto responses_section = section_offsets_.find(kResponsesSection);
    DCHECK(responses_section != section_offsets_.end());
    const uint64_t responses_section_offset = responses_section->second.first;
    const uint64_t responses_section_length = responses_section->second.second;

    const uint64_t length =
        std::min(responses_section_length, kMaxCBORItemHeaderSize);
    data_source_->Read(
        responses_section_offset, length,
        base::BindOnce(&MetadataParser::ParseResponsesSectionHeader,
                       weak_factory_.GetWeakPtr(),
                       std::move(*index_section_value), length));
  }

  // Step 2- of
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#index-section
  void ParseResponsesSectionHeader(
      cbor::Value index_section_value,
      uint64_t expected_data_length,
      const base::Optional<std::vector<uint8_t>>& data) {
    const cbor::Value::ArrayValue& index_section_array =
        index_section_value.GetArray();

    // Step 2.1. "Seek to offset sectionOffsets["responses"].offset in stream.
    // If this fails, return an error."
    if (!data || data->size() != expected_data_length) {
      RunCallbackAndDestroy(nullptr, "Error reading responses section header.");
      return;
    }
    InputReader input(*data);

    // Step 2.2. "Let (responsesType, numResponses) be the result of parsing the
    // type and argument of a CBOR item from the stream (Section 3.5.3). If this
    // returns an error, return that error."
    auto num_responses = input.ReadCBORHeader(CBORType::kArray);
    if (!num_responses) {
      RunCallbackAndDestroy(
          nullptr,
          "Cannot parse the number of elements of the responses section.");
      return;
    }
    // Step 2.3. "If responsesType is not 4 (a CBOR array) or numResponses is
    // not half of the length of index, return an error."
    if (index_section_array.size() % 2 != 0 ||
        *num_responses != index_section_array.size() / 2) {
      RunCallbackAndDestroy(
          nullptr, "Wrong number of elements in the responses section.");
      return;
    }

    // Step 3. "Let currentOffset be the current offset within stream minus
    // sectionOffsets["responses"].offset."
    uint64_t current_offset = input.CurrentOffset();

    // Step 4. "Let requests be an initially-empty map ([INFRA]) from HTTP
    // requests ([FETCH]) to structs ([INFRA]) with items named "offset" and
    // "length"."

    // Step 5. "For each (cbor-http-request, length) pair of adjacent elements
    // in index:"
    for (size_t i = 0; i < index_section_array.size(); i += 2) {
      const cbor::Value& cbor_http_request_value = index_section_array[i];
      const cbor::Value& length_value = index_section_array[i + 1];

      // Step 5.1. "Let (headers, pseudos) be the result of converting
      // cbor-http-request to a header list and pseudoheaders using the
      // algorithm in Section 3.6. If this returns an error, return that error."
      auto parsed_headers = ConvertCBORValueToHeaders(cbor_http_request_value);
      if (!parsed_headers) {
        RunCallbackAndDestroy(nullptr,
                              "Cannot parse headers in the index section.");
        return;
      }

      // Step 5.2. "If pseudos does not have keys named ':method' and
      // ':url', or its size isn't 2, return an error."
      const auto pseudo_method = parsed_headers->pseudos.find(":method");
      const auto pseudo_url = parsed_headers->pseudos.find(":url");
      if (parsed_headers->pseudos.size() != 2 ||
          pseudo_method == parsed_headers->pseudos.end() ||
          pseudo_url == parsed_headers->pseudos.end()) {
        RunCallbackAndDestroy(nullptr,
                              "Request headers map must have exactly two "
                              "pseudo-headers, :method and :url.");
        return;
      }

      // Step 5.3. "If pseudos[':method'] is not 'GET', return an error."
      if (pseudo_method->second != "GET") {
        RunCallbackAndDestroy(nullptr, "Request method must be GET.");
        return;
      }

      // Step 5.4. "Let parsedUrl be the result of parsing ([URL])
      // pseudos[':url'] with no base URL."
      GURL parsed_url(pseudo_url->second);

      // Step 5.5. "If parsedUrl is a failure, its fragment is not null, or it
      // includes credentials, return an error."
      if (!parsed_url.is_valid() || parsed_url.has_ref() ||
          parsed_url.has_username() || parsed_url.has_password()) {
        RunCallbackAndDestroy(nullptr,
                              ":url in header map must be a valid URL without "
                              "fragment or credentials.");
        return;
      }

      // Step 5.6. "Let http-request be a new request ([FETCH]) whose:
      // - method is pseudos[':method'],
      // - url is parsedUrl,
      // - header list is headers, and
      // - client is null."
      mojom::BundleIndexItemPtr item = mojom::BundleIndexItem::New();
      item->request_method = pseudo_method->second;
      item->request_url = std::move(parsed_url);
      item->request_headers = std::move(parsed_headers->headers);

      // Step 5.7. "Let responseOffset be sectionOffsets["responses"].offset +
      // currentOffset. This is relative to the start of the stream."
      const uint64_t response_offset =
          section_offsets_[kResponsesSection].first + current_offset;

      // Step 5.8. "If currentOffset + length is greater than
      // sectionOffsets["responses"].length, return an error."
      if (!length_value.is_unsigned()) {
        RunCallbackAndDestroy(
            nullptr,
            "Length value in the index section should be an unsigned.");
        return;
      }
      const uint64_t length = length_value.GetUnsigned();

      uint64_t response_end;
      if (!base::CheckAdd(current_offset, length)
               .AssignIfValid(&response_end) ||
          response_end > section_offsets_[kResponsesSection].second) {
        RunCallbackAndDestroy(nullptr,
                              "Index map is invalid: total length of responses "
                              "exceeds the length of the responses section.");
        return;
      }

      // Step 5.9. "If requests[http-request] exists, return an error. That is,
      // duplicate requests are forbidden."

      // TODO(crbug.com/966753): Implement this check.

      // Step 5.10. "Set requests[http-request] to a struct whose "offset"
      // item is responseOffset and whose "length" item is length."
      item->response_offset = response_offset;
      item->response_length = length;
      items_.push_back(std::move(item));

      // Step 5.11. "Set currentOffset to currentOffset + length."
      current_offset = response_end;
    }

    // We're done.
    RunCallbackAndDestroy(
        mojom::BundleMetadata::New(std::move(items_), manifest_url_),
        base::nullopt);
  }

  void RunCallbackAndDestroy(mojom::BundleMetadataPtr metadata,
                             const base::Optional<std::string>& error) {
    DCHECK((metadata && !error) || (!metadata && error));
    std::move(callback_).Run(std::move(metadata), error);
    delete this;
  }

  // Implements SharedBundleDataSource::Observer.
  void OnDisconnect() override {
    RunCallbackAndDestroy(nullptr, "Data source disconnected.");
  }

  scoped_refptr<SharedBundleDataSource> data_source_;
  ParseMetadataCallback callback_;
  uint64_t size_;
  // name -> (offset, length)
  std::map<std::string, std::pair<uint64_t, uint64_t>> section_offsets_;
  std::vector<mojom::BundleIndexItemPtr> items_;
  GURL manifest_url_;

  base::WeakPtrFactory<MetadataParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MetadataParser);
};

// A parser for reading single item from the responses section. This class owns
// itself and will self destruct after calling the ParseResponseCallback.
class BundledExchangesParser::ResponseParser
    : public BundledExchangesParser::SharedBundleDataSource::Observer {
 public:
  ResponseParser(scoped_refptr<SharedBundleDataSource> data_source,
                 uint64_t response_offset,
                 uint64_t response_length,
                 BundledExchangesParser::ParseResponseCallback callback)
      : data_source_(data_source),
        response_offset_(response_offset),
        response_length_(response_length),
        callback_(std::move(callback)) {
    data_source_->AddObserver(this);
  }
  ~ResponseParser() override { data_source_->RemoveObserver(this); }

  void Start(uint64_t buffer_size = kInitialBufferSizeForResponse) {
    const uint64_t length = std::min(response_length_, buffer_size);
    data_source_->Read(response_offset_, length,
                       base::BindOnce(&ResponseParser::ParseResponseHeader,
                                      weak_factory_.GetWeakPtr(), length));
  }

 private:
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-response
  void ParseResponseHeader(uint64_t expected_data_length,
                           const base::Optional<std::vector<uint8_t>>& data) {
    // Step 1. "Seek to offset requestMetadata.offset in stream. If this fails,
    // return an error."
    if (!data || data->size() != expected_data_length) {
      RunCallbackAndDestroy(nullptr, "Error reading response header.");
      return;
    }
    InputReader input(*data);

    // Step 2. "Read 1 byte from stream. If this is an error or isn't 0x82,
    // return an error."
    auto num_elements = input.ReadCBORHeader(CBORType::kArray);
    if (!num_elements || *num_elements != 2) {
      RunCallbackAndDestroy(nullptr, "Array size of response must be 2.");
      return;
    }

    // Step 3. "Let headerLength be the result of getting the length of a CBOR
    // bytestring header from stream (Section 3.5.2). If headerLength is an
    // error, return that error."
    auto header_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!header_length) {
      RunCallbackAndDestroy(nullptr, "Cannot parse response header length.");
      return;
    }

    // Step 4. "If headerLength is 524288 (512*1024) or greater, return an
    // error."
    if (*header_length >= kMaxResponseHeaderLength) {
      RunCallbackAndDestroy(nullptr, "Response header is too big.");
      return;
    }

    // If we don't have enough data for the headers and the CBOR header of the
    // payload, re-read with a larger buffer size.
    const uint64_t required_buffer_size = std::min(
        input.CurrentOffset() + *header_length + kMaxCBORItemHeaderSize,
        response_length_);
    if (data->size() < required_buffer_size) {
      DVLOG(1) << "Re-reading response header with a buffer of size "
               << required_buffer_size;
      Start(required_buffer_size);
      return;
    }

    // Step 5. "Let headerCbor be the result of reading headerLength bytes from
    // stream and parsing a CBOR item from them matching the headers CDDL rule.
    // If either the read or parse returns an error, return that error."
    auto headers_bytes = input.ReadBytes(*header_length);
    if (!headers_bytes) {
      RunCallbackAndDestroy(nullptr, "Cannot read response headers.");
      return;
    }
    cbor::Reader::DecoderError error;
    base::Optional<cbor::Value> headers_value =
        cbor::Reader::Read(*headers_bytes, &error);
    if (!headers_value) {
      RunCallbackAndDestroy(nullptr, "Cannot parse response headers.");
      return;
    }

    // Step 6. "Let (headers, pseudos) be the result of converting headerCbor
    // to a header list and pseudoheaders using the algorithm in Section 3.6.
    // If this returns an error, return that error."
    auto parsed_headers = ConvertCBORValueToHeaders(*headers_value);
    if (!parsed_headers) {
      RunCallbackAndDestroy(nullptr, "Cannot parse response headers.");
      return;
    }

    // Step 7. "If pseudos does not have a key named ':status' or its size
    // isn't 1, return an error."
    const auto pseudo_status = parsed_headers->pseudos.find(":status");
    if (parsed_headers->pseudos.size() != 1 ||
        pseudo_status == parsed_headers->pseudos.end()) {
      RunCallbackAndDestroy(
          nullptr,
          "Response headers map must have exactly one pseudo-header, :status.");
      return;
    }

    // Step 8. "If pseudos[':status'] isn't exactly 3 ASCII decimal digits,
    // return an error."
    int status;
    const auto& status_str = pseudo_status->second;
    if (status_str.size() != 3 ||
        !std::all_of(status_str.begin(), status_str.end(),
                     base::IsAsciiDigit<char>) ||
        !base::StringToInt(status_str, &status)) {
      RunCallbackAndDestroy(nullptr, ":status must be 3 ASCII decimal digits.");
      return;
    }

    // Step 9. "If headers does not contain a Content-Type header, return an
    // error."

    // TODO(crbug.com/966753): Implement this once
    // https://github.com/WICG/webpackage/issues/445 is resolved.

    // Step 10. "Let payloadLength be the result of getting the length of a CBOR
    // bytestring header from stream (Section 3.5.2). If payloadLength is an
    // error, return that error."
    auto payload_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!payload_length) {
      RunCallbackAndDestroy(nullptr, "Cannot parse response payload length.");
      return;
    }

    // Step 11. "If stream.currentOffset + payloadLength !=
    // requestMetadata.offset + requestMetadata.length, return an error."
    if (input.CurrentOffset() + *payload_length != response_length_) {
      RunCallbackAndDestroy(nullptr, "Unexpected payload length.");
      return;
    }

    // Step 12. "Let body be a new body ([FETCH]) whose stream is a tee'd copy
    // of stream starting at the current offset and ending after payloadLength
    // bytes."

    // Step 13. "Let response be a new response ([FETCH]) whose:
    // - Url list is request's url list,
    // - status is pseudos[':status'],
    // - header list is headers, and
    // - body is body."
    mojom::BundleResponsePtr response = mojom::BundleResponse::New();
    response->response_code = status;
    response->response_headers = std::move(parsed_headers->headers);
    response->payload_offset = response_offset_ + input.CurrentOffset();
    response->payload_length = *payload_length;
    RunCallbackAndDestroy(std::move(response), base::nullopt);
  }

  void RunCallbackAndDestroy(mojom::BundleResponsePtr response,
                             const base::Optional<std::string>& error) {
    DCHECK((response && !error) || (!response && error));
    std::move(callback_).Run(std::move(response), error);
    delete this;
  }

  // Implements SharedBundleDataSource::Observer.
  void OnDisconnect() override {
    RunCallbackAndDestroy(nullptr, "Data source disconnected.");
  }

  scoped_refptr<SharedBundleDataSource> data_source_;
  uint64_t response_offset_;
  uint64_t response_length_;
  ParseResponseCallback callback_;

  base::WeakPtrFactory<ResponseParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResponseParser);
};

BundledExchangesParser::SharedBundleDataSource::SharedBundleDataSource(
    mojo::PendingRemote<mojom::BundleDataSource> pending_data_source)
    : data_source_(std::move(pending_data_source)) {
  data_source_.set_disconnect_handler(base::BindOnce(
      &SharedBundleDataSource::OnDisconnect, base::Unretained(this)));
}

void BundledExchangesParser::SharedBundleDataSource::AddObserver(
    Observer* observer) {
  DCHECK(observers_.end() == observers_.find(observer));
  observers_.insert(observer);
}

void BundledExchangesParser::SharedBundleDataSource::RemoveObserver(
    Observer* observer) {
  auto it = observers_.find(observer);
  DCHECK(observers_.end() != it);
  observers_.erase(it);
}

BundledExchangesParser::SharedBundleDataSource::~SharedBundleDataSource() =
    default;

void BundledExchangesParser::SharedBundleDataSource::OnDisconnect() {
  for (auto* observer : observers_)
    observer->OnDisconnect();
}

void BundledExchangesParser::SharedBundleDataSource::GetSize(
    GetSizeCallback callback) {
  data_source_->GetSize(std::move(callback));
}

void BundledExchangesParser::SharedBundleDataSource::Read(
    uint64_t offset,
    uint64_t length,
    ReadCallback callback) {
  data_source_->Read(offset, length, std::move(callback));
}

BundledExchangesParser::BundledExchangesParser(
    mojo::PendingReceiver<mojom::BundledExchangesParser> receiver,
    mojo::PendingRemote<mojom::BundleDataSource> data_source)
    : receiver_(this, std::move(receiver)),
      data_source_(base::MakeRefCounted<SharedBundleDataSource>(
          std::move(data_source))) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &base::DeletePointer<BundledExchangesParser>, base::Unretained(this)));
}

BundledExchangesParser::~BundledExchangesParser() = default;

void BundledExchangesParser::ParseMetadata(ParseMetadataCallback callback) {
  MetadataParser* parser =
      new MetadataParser(data_source_, std::move(callback));
  parser->Start();
}

void BundledExchangesParser::ParseResponse(
    uint64_t response_offset,
    uint64_t response_length,
    ParseResponseCallback callback) {
  ResponseParser* parser = new ResponseParser(
      data_source_, response_offset, response_length, std::move(callback));
  parser->Start();
}

}  // namespace data_decoder
