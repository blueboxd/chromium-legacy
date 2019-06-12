// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/ipp_validator.h"

#include <cups/cups.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/http/http_util.h"
#include "printing/backend/cups_ipp_util.h"

namespace cups_proxy {
namespace {

// Initial version only supports english lcoales.
// TODO(crbug.com/945409): Extending to supporting arbitrary locales.
const char kLocaleEnglish[] = "en";

// Following ParseXxx methods translate mojom objects representing IPP
// attribute values to formats libCUPS APIs accept.

// Converting to vector<char> for libCUPS API:
// ippAddBooleans(..., int num_values, const char *values)
// TODO(crbug.com/945409): Convert cups_ipp_parser::mojom::Value to using
// mojo_base::Value.
base::Optional<std::vector<char>> ParseBooleans(
    const std::vector<cups_ipp_parser::mojom::ValuePtr>& values) {
  std::vector<char> ret;
  for (auto& value : values) {
    if (!value->is_bool_value()) {
      return base::nullopt;
    }

    ret.push_back(value->get_bool_value() ? 1 : 0);
  }
  return ret;
}

// Converting to vector<int> for libCUPS API:
// ippAddIntegers(..., int num_values, const int *values)
base::Optional<std::vector<int>> ParseIntegers(
    const std::vector<cups_ipp_parser::mojom::ValuePtr>& values) {
  std::vector<int> ret;
  for (auto& value : values) {
    if (!value->is_int_value()) {
      return base::nullopt;
    }

    ret.push_back(value->get_int_value());
  }
  return ret;
}

// Converting to vector<const char*> for libCUPS API:
// ippAddStrings(..., int num_values, const char *const *values)
base::Optional<std::vector<const char*>> ParseStrings(
    const std::vector<cups_ipp_parser::mojom::ValuePtr>& values) {
  std::vector<const char*> ret;
  for (auto& value : values) {
    if (!value->is_string_value()) {
      return base::nullopt;
    }

    // ret's cstrings reference |values| strings, so |values| MUST outlive it.
    ret.push_back(value->get_string_value().c_str());
  }
  return ret;
}

}  // namespace

// Verifies that |method|, |endpoint|, and |http_version| form a valid HTTP
// request-line. On success, returns a wrapper obj containing the verified
// request-line.
base::Optional<HttpRequestLine> IppValidator::ValidateHttpRequestLine(
    base::StringPiece method,
    base::StringPiece endpoint,
    base::StringPiece http_version) {
  if (method != "POST") {
    return base::nullopt;
  }
  if (http_version != "HTTP/1.1") {
    return base::nullopt;
  }

  // Ensure endpoint is either default('/') or known printer
  auto printer = delegate_->GetPrinter(endpoint.as_string());
  if (endpoint != "/" && !printer) {
    return base::nullopt;
  }

  return HttpRequestLine{method.as_string(), endpoint.as_string(),
                         http_version.as_string()};
}

base::Optional<std::vector<ipp_converter::HttpHeader>>
IppValidator::ValidateHttpHeaders(
    const base::flat_map<std::string, std::string>& headers) {
  // Sane, character-set checks.
  for (const auto& header : headers) {
    if (!net::HttpUtil::IsValidHeaderName(header.first) ||
        !net::HttpUtil::IsValidHeaderValue(header.second)) {
      return base::nullopt;
    }
  }

  return std::vector<ipp_converter::HttpHeader>(headers.begin(), headers.end());
}

ipp_t* IppValidator::ValidateIppMessage(
    cups_ipp_parser::mojom::IppMessagePtr ipp_message) {
  printing::ScopedIppPtr ipp = printing::WrapIpp(ippNew());

  // Fill ids.
  if (!ippSetVersion(ipp.get(), ipp_message->major_version,
                     ipp_message->minor_version)) {
    return nullptr;
  }

  if (!ippSetOperation(ipp.get(),
                       static_cast<ipp_op_t>(ipp_message->operation_id))) {
    return nullptr;
  }

  if (!ippSetRequestId(ipp.get(), ipp_message->request_id)) {
    return nullptr;
  }

  // Fill attributes.
  for (size_t i = 0; i < ipp_message->attributes.size(); ++i) {
    cups_ipp_parser::mojom::IppAttributePtr attribute =
        std::move(ipp_message->attributes[i]);

    switch (attribute->type) {
      case cups_ipp_parser::mojom::ValueType::BOOLEAN: {
        base::Optional<std::vector<char>> values =
            ParseBooleans(attribute->values);
        if (!values.has_value()) {
          return nullptr;
        }

        auto* attr = ippAddBooleans(
            ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
            attribute->name.c_str(), values->size(), values->data());
        if (!attr) {
          return nullptr;
        }
        break;
      }
      // TODO(crbug.com/945409): Include for multiple value checking.
      case cups_ipp_parser::mojom::ValueType::DATE: {
        if (!attribute->values.front()->is_char_value()) {
          return nullptr;
        }

        auto date = attribute->values.front()->get_char_value();
        auto* attr = ippAddDate(
            ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
            attribute->name.c_str(), static_cast<ipp_uchar_t*>(&date));
        if (!attr) {
          return nullptr;
        }
        break;
      }
      case cups_ipp_parser::mojom::ValueType::INTEGER: {
        base::Optional<std::vector<int>> values =
            ParseIntegers(attribute->values);
        if (!values.has_value()) {
          return nullptr;
        }

        auto* attr = ippAddIntegers(
            ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
            static_cast<ipp_tag_t>(attribute->value_tag),
            attribute->name.c_str(), values->size(), values->data());
        if (!attr) {
          return nullptr;
        }
        break;
      }
      case cups_ipp_parser::mojom::ValueType::STRING: {
        // Note: cstrings_values references attribute->values, i.e.
        // cstrings_values MUST outlive attribute->values.
        base::Optional<std::vector<const char*>> cstrings_values =
            ParseStrings(attribute->values);
        if (!cstrings_values.has_value()) {
          return nullptr;
        }

        auto* attr = ippAddStrings(
            ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
            static_cast<ipp_tag_t>(attribute->value_tag),
            attribute->name.c_str(), cstrings_values->size(), kLocaleEnglish,
            cstrings_values->data());
        if (!attr) {
          return nullptr;
        }
        break;
      }
      default:
        NOTREACHED() << "Unknown IPP attribute type found.";
    }
  }

  // Validate Attributes.
  if (!ippValidateAttributes(ipp.get())) {
    return nullptr;
  }

  // Return built ipp object.
  return ipp.release();
}

// Requires IPP data portion is either empty or looks like a pdf.
bool IppValidator::ValidateIppData(const std::vector<uint8_t>& ipp_data) {
  const int pdf_magic_bytes_size = 4;
  constexpr std::array<uint8_t, pdf_magic_bytes_size> pdf_magic_bytes = {
      0x25, 0x50, 0x44, 0x46};  // { %PDF }

  // Empty IPP data portion.
  if (ipp_data.empty()) {
    return true;
  }

  if (ipp_data.size() < pdf_magic_bytes_size) {
    return false;
  }

  // Check that |ipp_data| starts with pdf_magic_bytes.
  return std::equal(ipp_data.begin(), ipp_data.begin() + pdf_magic_bytes_size,
                    pdf_magic_bytes.begin());
}

IppValidator::IppValidator(
    base::WeakPtr<chromeos::printing::CupsProxyServiceDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IppValidator::~IppValidator() = default;

base::Optional<IppRequest> IppValidator::ValidateIppRequest(
    cups_ipp_parser::mojom::IppRequestPtr to_validate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!delegate_) {
    // TODO(crbug/495409): Add fatal error option to bring down service.
    return base::nullopt;
  }

  // Build request line.
  auto request_line = ValidateHttpRequestLine(
      to_validate->method, to_validate->endpoint, to_validate->http_version);
  if (!request_line.has_value()) {
    return base::nullopt;
  }

  // Build headers.
  auto headers = ValidateHttpHeaders(to_validate->headers);
  if (!headers.has_value()) {
    return base::nullopt;
  }

  // Build ipp message.
  // Note: Moving ipp here, to_validate->ipp no longer valid below.
  printing::ScopedIppPtr ipp =
      printing::WrapIpp(ValidateIppMessage(std::move(to_validate->ipp)));
  if (ipp == nullptr) {
    return base::nullopt;
  }

  // Validate ipp data.
  // TODO(crbug/894607): Validate ippData (pdf).
  if (!ValidateIppData(to_validate->data)) {
    return base::nullopt;
  }

  // Marshall request
  IppRequest ret;
  ret.request_line = std::move(*request_line);
  ret.headers = std::move(*headers);
  ret.ipp = std::move(ipp);
  ret.ipp_data = std::move(to_validate->data);

  // Build parsed request buffer.
  auto request_buffer = ipp_converter::BuildIppRequest(
      ret.request_line.method, ret.request_line.endpoint,
      ret.request_line.http_version, ret.headers, ret.ipp.get(), ret.ipp_data);
  if (!request_buffer.has_value()) {
    return base::nullopt;
  }

  ret.buffer = std::move(*request_buffer);
  return ret;
}

}  // namespace cups_proxy
