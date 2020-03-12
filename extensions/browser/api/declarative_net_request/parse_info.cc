// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/parse_info.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "extensions/common/error_utils.h"

namespace extensions {
namespace declarative_net_request {

namespace {

// Helper to ensure pointers to string literals can be used with
// base::JoinString.
std::string JoinString(base::span<const char* const> parts) {
  std::vector<base::StringPiece> parts_piece;
  for (const char* part : parts)
    parts_piece.push_back(part);
  return base::JoinString(parts_piece, ", ");
}

}  // namespace

ParseInfo::ParseInfo() = default;
ParseInfo::ParseInfo(ParseInfo&&) = default;
ParseInfo& ParseInfo::operator=(ParseInfo&&) = default;
ParseInfo::~ParseInfo() = default;

void ParseInfo::AddRegexLimitExceededRule(int rule_id) {
  DCHECK(!has_error_);
  regex_limit_exceeded_rules_.push_back(rule_id);
}

void ParseInfo::SetError(ParseResult error_reason, const int* rule_id) {
  has_error_ = true;
  error_reason_ = error_reason;

  // Every error except ERROR_PERSISTING_RULESET requires |rule_id|.
  DCHECK_EQ(!rule_id, error_reason == ParseResult::ERROR_PERSISTING_RULESET);

  switch (error_reason) {
    case ParseResult::NONE:
      NOTREACHED();
      break;
    case ParseResult::SUCCESS:
      NOTREACHED();
      break;
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      error_ = ErrorUtils::FormatErrorMessage(kErrorResourceTypeDuplicated,
                                              base::NumberToString(*rule_id));
      break;
    case ParseResult::ERROR_INVALID_RULE_ID:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(*rule_id), kIDKey,
          base::NumberToString(kMinValidID));
      break;
    case ParseResult::ERROR_EMPTY_RULE_PRIORITY:
      error_ = ErrorUtils::FormatErrorMessage(kErrorEmptyRulePriority,
                                              base::NumberToString(*rule_id));
      break;
    case ParseResult::ERROR_INVALID_RULE_PRIORITY:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(*rule_id), kPriorityKey,
          base::NumberToString(kMinValidPriority));
      break;
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      error_ = ErrorUtils::FormatErrorMessage(kErrorNoApplicableResourceTypes,

                                              base::NumberToString(*rule_id));
      break;
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id), kDomainsKey);
      break;
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id), kResourceTypesKey);
      break;
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(*rule_id), kUrlFilterKey);
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      error_ = ErrorUtils::FormatErrorMessage(kErrorInvalidRedirectUrl,
                                              base::NumberToString(*rule_id),
                                              kRedirectUrlPath);
      break;
    case ParseResult::ERROR_DUPLICATE_IDS:
      error_ = ErrorUtils::FormatErrorMessage(kErrorDuplicateIDs,
                                              base::NumberToString(*rule_id));
      break;
    case ParseResult::ERROR_PERSISTING_RULESET:
      error_ = kErrorPersisting;
      break;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kUrlFilterKey);
      break;
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kDomainsKey);
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kExcludedDomainsKey);
      break;
    case ParseResult::ERROR_INVALID_URL_FILTER:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kUrlFilterKey);
      break;
    case ParseResult::ERROR_EMPTY_REMOVE_HEADERS_LIST:
      error_ = ErrorUtils::FormatErrorMessage(kErrorEmptyRemoveHeadersList,
                                              base::NumberToString(*rule_id),
                                              kRemoveHeadersListKey);
      break;
    case ParseResult::ERROR_INVALID_REDIRECT:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kRedirectPath);
      break;
    case ParseResult::ERROR_INVALID_EXTENSION_PATH:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kExtensionPathPath);
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_SCHEME:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidTransformScheme, base::NumberToString(*rule_id),
          kTransformSchemePath,
          JoinString(base::span<const char* const>(kAllowedTransformSchemes)));
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_PORT:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kTransformPortPath);
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_QUERY:
      error_ = ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                              base::NumberToString(*rule_id),
                                              kTransformQueryPath);
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT:
      error_ = ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                              base::NumberToString(*rule_id),
                                              kTransformFragmentPath);
      break;
    case ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorQueryAndTransformBothSpecified, base::NumberToString(*rule_id),
          kTransformQueryPath, kTransformQueryTransformPath);
      break;
    case ParseResult::ERROR_JAVASCRIPT_REDIRECT:
      error_ = ErrorUtils::FormatErrorMessage(kErrorJavascriptRedirect,
                                              base::NumberToString(*rule_id),
                                              kRedirectUrlPath);
      break;
    case ParseResult::ERROR_EMPTY_REGEX_FILTER:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(*rule_id), kRegexFilterKey);
      break;
    case ParseResult::ERROR_NON_ASCII_REGEX_FILTER:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kRegexFilterKey);
      break;
    case ParseResult::ERROR_INVALID_REGEX_FILTER:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kRegexFilterKey);
      break;
    case ParseResult::ERROR_NO_HEADERS_SPECIFIED:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorNoHeaderListsSpecified, base::NumberToString(*rule_id),
          kRequestHeadersPath, kResponseHeadersPath);
      break;
    case ParseResult::ERROR_EMPTY_REQUEST_HEADERS_LIST:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id), kRequestHeadersPath);
      break;
    case ParseResult::ERROR_EMPTY_RESPONSE_HEADERS_LIST:
      error_ = ErrorUtils::FormatErrorMessage(kErrorEmptyList,
                                              base::NumberToString(*rule_id),
                                              kResponseHeadersPath);
      break;
    case ParseResult::ERROR_INVALID_HEADER_NAME:
      error_ = ErrorUtils::FormatErrorMessage(kErrorInvalidHeaderName,
                                              base::NumberToString(*rule_id));
      break;
    case ParseResult::ERROR_REGEX_TOO_LARGE:
      // These rules are ignored while indexing and so SetError won't be called
      // for them. See AddRegexLimitExceededRule().
      NOTREACHED();
      break;
    case ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED:
      error_ = ErrorUtils::FormatErrorMessage(kErrorMultipleFilters,
                                              base::NumberToString(*rule_id),
                                              kUrlFilterKey, kRegexFilterKey);
      break;
    case ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorRegexSubstitutionWithoutFilter, base::NumberToString(*rule_id),
          kRegexSubstitutionKey, kRegexFilterKey);
      break;
    case ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION:
      error_ = ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                              base::NumberToString(*rule_id),
                                              kRegexSubstitutionPath);
      break;
    case ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE:
      error_ = ErrorUtils::FormatErrorMessage(
          kErrorInvalidAllowAllRequestsResourceType,
          base::NumberToString(*rule_id));
      break;
  }
}

}  // namespace declarative_net_request
}  // namespace extensions
