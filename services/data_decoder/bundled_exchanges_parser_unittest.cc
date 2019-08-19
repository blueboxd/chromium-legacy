// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/bundled_exchanges_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/cbor/writer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

namespace {

constexpr char kFallbackUrl[] = "https://test.example.com/";
constexpr char kManifestUrl[] = "https://test.example.com/manifest";

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(base::FilePath(
      FILE_PATH_LITERAL("services/test/data/bundled_exchanges")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

class TestDataSource : public mojom::BundleDataSource {
 public:
  explicit TestDataSource(const base::FilePath& path)
      : data_(GetTestFileContents(path)) {}
  explicit TestDataSource(const std::vector<uint8_t>& data)
      : data_(reinterpret_cast<const char*>(data.data()), data.size()) {}

  void GetSize(GetSizeCallback callback) override {
    std::move(callback).Run(data_.size());
  }

  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    if (offset + length > data_.size())
      std::move(callback).Run(base::nullopt);
    const uint8_t* start =
        reinterpret_cast<const uint8_t*>(data_.data()) + offset;
    std::move(callback).Run(std::vector<uint8_t>(start, start + length));
  }

  base::StringPiece GetPayload(const mojom::BundleResponsePtr& response) {
    return base::StringPiece(data_).substr(response->payload_offset,
                                           response->payload_length);
  }

  void AddReceiver(mojo::PendingReceiver<mojom::BundleDataSource> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  std::string data_;
  mojo::ReceiverSet<mojom::BundleDataSource> receivers_;
};

using ParseBundleResult =
    std::pair<mojom::BundleMetadataPtr, mojom::BundleMetadataParseErrorPtr>;

ParseBundleResult ParseBundle(TestDataSource* data_source) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::BundledExchangesParser> parser_remote;
  BundledExchangesParser parser_impl(
      parser_remote.InitWithNewPipeAndPassReceiver(), std::move(source_remote));
  data_decoder::mojom::BundledExchangesParser& parser = parser_impl;

  base::RunLoop run_loop;
  ParseBundleResult result;
  parser.ParseMetadata(base::BindLambdaForTesting(
      [&result, &run_loop](mojom::BundleMetadataPtr metadata,
                           mojom::BundleMetadataParseErrorPtr error) {
        result = std::make_pair(std::move(metadata), std::move(error));
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
  EXPECT_TRUE((result.first && !result.second) ||
              (!result.first && result.second));
  return result;
}

void ExpectFormatErrorWithFallbackURL(ParseBundleResult result) {
  ASSERT_TRUE(result.second);
  EXPECT_EQ(result.second->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_EQ(result.second->fallback_url, kFallbackUrl);
}

// Finds the only response for |url|. The index entry for |url| must not have
// variants-value.
mojom::BundleResponseLocationPtr FindResponse(
    const mojom::BundleMetadataPtr& metadata,
    const GURL& url) {
  const auto item = metadata->requests.find(url);
  if (item == metadata->requests.end())
    return nullptr;

  const mojom::BundleIndexValuePtr& index_value = item->second;
  EXPECT_TRUE(index_value->variants_value.empty());
  EXPECT_EQ(index_value->response_locations.size(), 1u);
  if (index_value->response_locations.empty())
    return nullptr;
  return index_value->response_locations[0].Clone();
}

mojom::BundleResponsePtr ParseResponse(
    TestDataSource* data_source,
    const mojom::BundleResponseLocationPtr& location) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::BundledExchangesParser> parser_remote;
  BundledExchangesParser parser_impl(
      parser_remote.InitWithNewPipeAndPassReceiver(), std::move(source_remote));
  data_decoder::mojom::BundledExchangesParser& parser = parser_impl;

  base::RunLoop run_loop;
  mojom::BundleResponsePtr result;
  parser.ParseResponse(
      location->offset, location->length,
      base::BindLambdaForTesting(
          [&result, &run_loop](mojom::BundleResponsePtr response,
                               mojom::BundleResponseParseErrorPtr error) {
            result = std::move(response);
            run_loop.QuitClosure().Run();
          }));
  run_loop.Run();
  return result;
}

class BundleBuilder {
 public:
  using Headers = std::vector<std::pair<std::string, std::string>>;
  struct ResponseLocation {
    // /components/cbor uses int64_t for integer types.
    int64_t offset;
    int64_t length;
  };

  BundleBuilder(const std::string& fallback_url,
                const std::string& manifest_url)
      : fallback_url_(fallback_url) {
    writer_config_.allow_invalid_utf8_for_testing = true;
    if (!manifest_url.empty()) {
      AddSection("manifest",
                 cbor::Value::InvalidUTF8StringValueForTesting(manifest_url));
    }
  }

  void AddExchange(base::StringPiece url,
                   const Headers& response_headers,
                   base::StringPiece payload) {
    AddIndexEntry(url, "", {AddResponse(response_headers, payload)});
  }

  ResponseLocation AddResponse(const Headers& headers,
                               base::StringPiece payload) {
    // We assume that the size of the CBOR header of the responses array is 1,
    // which is true only if the responses array has no more than 23 elements.
    EXPECT_LT(responses_.size(), 23u)
        << "BundleBuilder cannot create bundles with more than 23 responses";

    cbor::Value::ArrayValue response_array;
    response_array.emplace_back(Encode(CreateHeaderMap(headers)));
    response_array.emplace_back(CreateByteString(payload));
    cbor::Value response(response_array);
    int64_t response_length = EncodedLength(response);
    ResponseLocation result = {current_responses_offset_, response_length};
    current_responses_offset_ += response_length;
    responses_.emplace_back(std::move(response));
    return result;
  }

  void AddIndexEntry(base::StringPiece url,
                     base::StringPiece variants_value,
                     std::vector<ResponseLocation> response_locations) {
    cbor::Value::ArrayValue index_value_array;
    index_value_array.emplace_back(CreateByteString(variants_value));
    for (const auto& location : response_locations) {
      index_value_array.emplace_back(location.offset);
      index_value_array.emplace_back(location.length);
    }
    index_.insert({cbor::Value::InvalidUTF8StringValueForTesting(url),
                   cbor::Value(index_value_array)});
  }

  void AddSection(base::StringPiece name, cbor::Value section) {
    section_lengths_.emplace_back(name);
    section_lengths_.emplace_back(EncodedLength(section));
    sections_.emplace_back(std::move(section));
  }

  std::vector<uint8_t> CreateBundle() {
    AddSection("index", cbor::Value(index_));
    AddSection("responses", cbor::Value(responses_));
    return Encode(CreateTopLevel());
  }

 private:
  static cbor::Value CreateByteString(base::StringPiece s) {
    return cbor::Value(base::as_bytes(base::make_span(s)));
  }

  static cbor::Value CreateHeaderMap(const Headers& headers) {
    cbor::Value::MapValue map;
    for (const auto& pair : headers)
      map.insert({CreateByteString(pair.first), CreateByteString(pair.second)});
    return cbor::Value(std::move(map));
  }

  cbor::Value CreateTopLevel() {
    cbor::Value::ArrayValue toplevel_array;
    toplevel_array.emplace_back(
        CreateByteString(u8"\U0001F310\U0001F4E6"));  // "🌐📦"
    toplevel_array.emplace_back(
        CreateByteString(base::StringPiece("b1\0\0", 4)));
    toplevel_array.emplace_back(
        cbor::Value::InvalidUTF8StringValueForTesting(fallback_url_));
    toplevel_array.emplace_back(Encode(cbor::Value(section_lengths_)));
    toplevel_array.emplace_back(sections_);
    toplevel_array.emplace_back(CreateByteString(""));  // length (ignored)
    return cbor::Value(toplevel_array);
  }

  std::vector<uint8_t> Encode(const cbor::Value& value) {
    return *cbor::Writer::Write(value, writer_config_);
  }

  int64_t EncodedLength(const cbor::Value& value) {
    return Encode(value).size();
  }

  cbor::Writer::Config writer_config_;
  std::string fallback_url_;
  cbor::Value::ArrayValue section_lengths_;
  cbor::Value::ArrayValue sections_;
  cbor::Value::MapValue index_;
  cbor::Value::ArrayValue responses_;

  // 1 for the CBOR header byte. See the comment at the top of AddResponse().
  int64_t current_responses_offset_ = 1;
};

}  // namespace

class BundledExchangeParserTest : public testing::Test {
 private:
  base::test::ScopedTaskEnvironment task_environment_;
};

TEST_F(BundledExchangeParserTest, WrongMagic) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  bundle[3] ^= 1;
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_TRUE(error->fallback_url.is_empty());
}

TEST_F(BundledExchangeParserTest, UnknownVersion) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  // Modify the version string from "b1\0\0" to "q1\0\0".
  ASSERT_EQ(bundle[11], 'b');
  bundle[11] = 'q';
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kVersionError);
  EXPECT_EQ(error->fallback_url, kFallbackUrl);
}

TEST_F(BundledExchangeParserTest, FallbackURLIsNotUTF8) {
  BundleBuilder builder("https://test.example.com/\xcc", kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_TRUE(error->fallback_url.is_empty());
}

TEST_F(BundledExchangeParserTest, FallbackURLHasFragment) {
  BundleBuilder builder("https://test.example.com/#fragment", kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_TRUE(error->fallback_url.is_empty());
}

TEST_F(BundledExchangeParserTest, SectionLengthsTooLarge) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  std::string too_long_section_name(8192, 'x');
  builder.AddSection(too_long_section_name, cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, DuplicateSectionName) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddSection("foo", cbor::Value(0));
  builder.AddSection("foo", cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, SingleEntry) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(BundledExchangeParserTest, InvalidRequestURL) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("", {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, RequestURLIsNotUTF8) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/\xcc",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, RequestURLHasBadScheme) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("file:///tmp/foo",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, RequestURLHasCredentials) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://user:passwd@test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, RequestURLHasFragment) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/#fragment",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, NoStatusInResponseHeaders) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{"content-type", "text/plain"}},
                      "payload");  // ":status" is missing.
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(BundledExchangeParserTest, InvalidResponseStatus) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "0200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(BundledExchangeParserTest, ExtraPseudoInResponseHeaders) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange(
      "https://test.example.com/",
      {{":status", "200"}, {":foo", ""}, {"content-type", "text/plain"}},
      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(BundledExchangeParserTest, UpperCaseCharacterInHeaderName) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"Content-Type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(BundledExchangeParserTest, InvalidHeaderValue) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "\n"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(BundledExchangeParserTest, NoContentTypeWithNonEmptyContent) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/", {{":status", "200"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(BundledExchangeParserTest, NoContentTypeWithEmptyContent) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/", {{":status", "301"}}, "");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_TRUE(ParseResponse(&data_source, location));
}

TEST_F(BundledExchangeParserTest, Variants) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  auto location1 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/html"}}, "payload1");
  auto location2 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload2");
  builder.AddIndexEntry("https://test.example.com/",
                        "Accept;text/html;text/plain", {location1, location2});
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  const auto& found =
      metadata->requests.find(GURL("https://test.example.com/"));
  ASSERT_NE(found, metadata->requests.end());
  const mojom::BundleIndexValuePtr& index_entry = found->second;
  EXPECT_EQ(index_entry->variants_value, "Accept;text/html;text/plain");
  ASSERT_EQ(index_entry->response_locations.size(), 2u);

  auto response1 =
      ParseResponse(&data_source, index_entry->response_locations[0]);
  ASSERT_TRUE(response1);
  EXPECT_EQ(data_source.GetPayload(response1), "payload1");
  auto response2 =
      ParseResponse(&data_source, index_entry->response_locations[1]);
  ASSERT_TRUE(response2);
  EXPECT_EQ(data_source.GetPayload(response2), "payload2");
}

TEST_F(BundledExchangeParserTest, EmptyIndexEntry) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddIndexEntry("https://test.example.com/", "", {});
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, EmptyIndexEntryWithVariants) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddIndexEntry("https://test.example.com/",
                        "Accept;text/html;text/plain", {});
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, MultipleResponsesWithoutVariantsValue) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  auto location1 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/html"}}, "payload1");
  auto location2 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload2");
  builder.AddIndexEntry("https://test.example.com/", "",
                        {location1, location2});
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, AllKnownSectionInCritical) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("manifest");
  critical_section.emplace_back("index");
  critical_section.emplace_back("critical");
  critical_section.emplace_back("responses");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
}

TEST_F(BundledExchangeParserTest, UnknownSectionInCritical) {
  BundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("unknown_section_name");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, NoManifest) {
  BundleBuilder builder(kFallbackUrl, std::string());
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, InvalidManifestURL) {
  BundleBuilder builder(kFallbackUrl, "not-an-absolute-url");
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, ParseGoldenFile) {
  TestDataSource data_source(base::FilePath(FILE_PATH_LITERAL("hello.wbn")));

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 4u);
  EXPECT_EQ(metadata->manifest_url,
            "https://test.example.org/manifest.webmanifest");

  std::map<std::string, mojom::BundleResponsePtr> responses;
  for (const auto& item : metadata->requests) {
    auto location = FindResponse(metadata, item.first);
    ASSERT_TRUE(location);
    auto resp = ParseResponse(&data_source, location);
    ASSERT_TRUE(resp);
    responses[item.first.spec()] = std::move(resp);
  }

  ASSERT_TRUE(responses["https://test.example.org/"]);
  EXPECT_EQ(responses["https://test.example.org/"]->response_code, 200);
  EXPECT_EQ(
      responses["https://test.example.org/"]->response_headers["content-type"],
      "text/html; charset=utf-8");
  EXPECT_EQ(data_source.GetPayload(responses["https://test.example.org/"]),
            GetTestFileContents(
                base::FilePath(FILE_PATH_LITERAL("hello/index.html"))));

  EXPECT_TRUE(responses["https://test.example.org/index.html"]);
  EXPECT_TRUE(responses["https://test.example.org/manifest.webmanifest"]);
  EXPECT_TRUE(responses["https://test.example.org/script.js"]);
}

// TODO(crbug.com/969596): Add a test case that loads a wbn file with variants,
// once gen-bundle supports variants.

}  // namespace data_decoder
