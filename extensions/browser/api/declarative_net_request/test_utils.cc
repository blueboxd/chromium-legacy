// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/test_utils.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {

namespace dnr_api = api::declarative_net_request;

RequestAction CreateRequestActionForTesting(RequestAction::Type type,
                                            uint32_t rule_id,
                                            uint32_t rule_priority,
                                            dnr_api::SourceType source_type,
                                            const ExtensionId& extension_id) {
  dnr_api::RuleActionType action = [type] {
    switch (type) {
      case RequestAction::Type::BLOCK:
      case RequestAction::Type::COLLAPSE:
        return dnr_api::RULE_ACTION_TYPE_BLOCK;
      case RequestAction::Type::ALLOW:
        return dnr_api::RULE_ACTION_TYPE_ALLOW;
      case RequestAction::Type::REDIRECT:
        return dnr_api::RULE_ACTION_TYPE_REDIRECT;
      case RequestAction::Type::UPGRADE:
        return dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME;
      case RequestAction::Type::REMOVE_HEADERS:
        return dnr_api::RULE_ACTION_TYPE_REMOVEHEADERS;
      case RequestAction::Type::ALLOW_ALL_REQUESTS:
        return dnr_api::RULE_ACTION_TYPE_ALLOWALLREQUESTS;
    }
  }();
  return RequestAction(type, rule_id,
                       ComputeIndexedRulePriority(rule_priority, action),
                       source_type, extension_id);
}

// Note: This is not declared in the anonymous namespace so that we can use it
// with gtest. This reuses the logic used to test action equality in
// TestRequestACtion in test_utils.h.
bool operator==(const RequestAction& lhs, const RequestAction& rhs) {
  // TODO(crbug.com/947591): Modify this method for
  // flat::IndexType_modify_headers.
  static_assert(flat::IndexType_count == 6,
                "Modify this method to ensure it stays updated as new actions "
                "are added.");

  auto are_vectors_equal = [](std::vector<const char*> a,
                              std::vector<const char*> b) {
    return std::set<base::StringPiece>(a.begin(), a.end()) ==
           std::set<base::StringPiece>(b.begin(), b.end());
  };

  auto get_members_tuple = [](const RequestAction& action) {
    return std::tie(action.type, action.redirect_url, action.rule_id,
                    action.index_priority, action.source_type,
                    action.extension_id);
  };

  return get_members_tuple(lhs) == get_members_tuple(rhs) &&
         are_vectors_equal(lhs.request_headers_to_remove,
                           rhs.request_headers_to_remove) &&
         are_vectors_equal(lhs.response_headers_to_remove,
                           rhs.response_headers_to_remove);
}

std::ostream& operator<<(std::ostream& output, RequestAction::Type type) {
  switch (type) {
    case RequestAction::Type::BLOCK:
      output << "BLOCK";
      break;
    case RequestAction::Type::COLLAPSE:
      output << "COLLAPSE";
      break;
    case RequestAction::Type::ALLOW:
      output << "ALLOW";
      break;
    case RequestAction::Type::REDIRECT:
      output << "REDIRECT";
      break;
    case RequestAction::Type::UPGRADE:
      output << "UPGRADE";
      break;
    case RequestAction::Type::REMOVE_HEADERS:
      output << "REMOVE_HEADERS";
      break;
    case RequestAction::Type::ALLOW_ALL_REQUESTS:
      output << "ALLOW_ALL_REQUESTS";
      break;
  }
  return output;
}

std::ostream& operator<<(std::ostream& output, const RequestAction& action) {
  output << "\nRequestAction\n";
  output << "|type| " << action.type << "\n";
  output << "|redirect_url| "
         << (action.redirect_url ? action.redirect_url->spec()
                                 : std::string("nullopt"))
         << "\n";
  output << "|rule_id| " << action.rule_id << "\n";
  output << "|index_priority| " << action.index_priority << "\n";
  output << "|source_type| "
         << api::declarative_net_request::ToString(action.source_type) << "\n";
  output << "|extension_id| " << action.extension_id << "\n";
  output << "|request_headers_to_remove| "
         << ::testing::PrintToString(action.request_headers_to_remove) << "\n";
  output << "|response_headers_to_remove| "
         << ::testing::PrintToString(action.response_headers_to_remove);
  return output;
}

std::ostream& operator<<(std::ostream& output,
                         const base::Optional<RequestAction>& action) {
  if (!action)
    return output << "empty Optional<RequestAction>";
  return output << *action;
}

std::ostream& operator<<(std::ostream& output, const ParseResult& result) {
  switch (result) {
    case ParseResult::NONE:
      output << "NONE";
      break;
    case ParseResult::SUCCESS:
      output << "SUCCESS";
      break;
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      output << "ERROR_RESOURCE_TYPE_DUPLICATED";
      break;
    case ParseResult::ERROR_EMPTY_RULE_PRIORITY:
      output << "ERROR_EMPTY_RULE_PRIORITY";
      break;
    case ParseResult::ERROR_INVALID_RULE_ID:
      output << "ERROR_INVALID_RULE_ID";
      break;
    case ParseResult::ERROR_INVALID_RULE_PRIORITY:
      output << "ERROR_INVALID_RULE_PRIORITY";
      break;
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      output << "ERROR_NO_APPLICABLE_RESOURCE_TYPES";
      break;
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      output << "ERROR_EMPTY_DOMAINS_LIST";
      break;
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      output << "ERROR_EMPTY_RESOURCE_TYPES_LIST";
      break;
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      output << "ERROR_EMPTY_URL_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      output << "ERROR_INVALID_REDIRECT_URL";
      break;
    case ParseResult::ERROR_DUPLICATE_IDS:
      output << "ERROR_DUPLICATE_IDS";
      break;
    case ParseResult::ERROR_PERSISTING_RULESET:
      output << "ERROR_PERSISTING_RULESET";
      break;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      output << "ERROR_NON_ASCII_URL_FILTER";
      break;
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      output << "ERROR_NON_ASCII_DOMAIN";
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      output << "ERROR_NON_ASCII_EXCLUDED_DOMAIN";
      break;
    case ParseResult::ERROR_INVALID_URL_FILTER:
      output << "ERROR_INVALID_URL_FILTER";
      break;
    case ParseResult::ERROR_EMPTY_REMOVE_HEADERS_LIST:
      output << "ERROR_EMPTY_REMOVE_HEADERS_LIST";
      break;
    case ParseResult::ERROR_INVALID_REDIRECT:
      output << "ERROR_INVALID_REDIRECT";
      break;
    case ParseResult::ERROR_INVALID_EXTENSION_PATH:
      output << "ERROR_INVALID_EXTENSION_PATH";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_SCHEME:
      output << "ERROR_INVALID_TRANSFORM_SCHEME";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_PORT:
      output << "ERROR_INVALID_TRANSFORM_PORT";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_QUERY:
      output << "ERROR_INVALID_TRANSFORM_QUERY";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT:
      output << "ERROR_INVALID_TRANSFORM_FRAGMENT";
      break;
    case ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED:
      output << "ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED";
      break;
    case ParseResult::ERROR_JAVASCRIPT_REDIRECT:
      output << "ERROR_JAVASCRIPT_REDIRECT";
      break;
    case ParseResult::ERROR_EMPTY_REGEX_FILTER:
      output << "ERROR_EMPTY_REGEX_FILTER";
      break;
    case ParseResult::ERROR_NON_ASCII_REGEX_FILTER:
      output << "ERROR_NON_ASCII_REGEX_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REGEX_FILTER:
      output << "ERROR_INVALID_REGEX_FILTER";
      break;
    case ParseResult::ERROR_NO_HEADERS_SPECIFIED:
      output << "ERROR_NO_HEADERS_SPECIFIED";
      break;
    case ParseResult::ERROR_EMPTY_REQUEST_HEADERS_LIST:
      output << "ERROR_EMPTY_REQUEST_HEADERS_LIST";
      break;
    case ParseResult::ERROR_EMPTY_RESPONSE_HEADERS_LIST:
      output << "ERROR_EMPTY_RESPONSE_HEADERS_LIST";
      break;
    case ParseResult::ERROR_INVALID_HEADER_NAME:
      output << "ERROR_INVALID_HEADER_NAME";
      break;
    case ParseResult::ERROR_REGEX_TOO_LARGE:
      output << "ERROR_REGEX_TOO_LARGE";
      break;
    case ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED:
      output << "ERROR_MULTIPLE_FILTERS_SPECIFIED";
      break;
    case ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER:
      output << "ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION:
      output << "ERROR_INVALID_REGEX_SUBSTITUTION";
      break;
    case ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE:
      output << "ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE";
      break;
  }
  return output;
}

bool AreAllIndexedStaticRulesetsValid(
    const Extension& extension,
    content::BrowserContext* browser_context) {
  std::vector<RulesetSource> sources = RulesetSource::CreateStatic(extension);

  for (RulesetSource& source : sources) {
    int expected_checksum = -1;
    if (!ExtensionPrefs::Get(browser_context)
             ->GetDNRStaticRulesetChecksum(extension.id(), source.id(),
                                           &expected_checksum)) {
      return false;
    }

    std::unique_ptr<RulesetMatcher> matcher;
    if (RulesetMatcher::CreateVerifiedMatcher(std::move(source),
                                              expected_checksum, &matcher) !=
        RulesetMatcher::kLoadSuccess) {
      return false;
    }
  }

  return true;
}

bool CreateVerifiedMatcher(const std::vector<TestRule>& rules,
                           const RulesetSource& source,
                           std::unique_ptr<RulesetMatcher>* matcher,
                           int* expected_checksum) {
  // Serialize |rules|.
  ListBuilder builder;
  for (const auto& rule : rules)
    builder.Append(rule.ToValue());
  JSONFileValueSerializer(source.json_path()).Serialize(*builder.Build());

  // Index ruleset.
  IndexAndPersistJSONRulesetResult result =
      source.IndexAndPersistJSONRulesetUnsafe();
  if (!result.success) {
    DCHECK(result.error.empty()) << result.error;
    return false;
  }

  if (!result.warnings.empty())
    return false;

  if (expected_checksum)
    *expected_checksum = result.ruleset_checksum;

  // Create verified matcher.
  RulesetMatcher::LoadRulesetResult load_result =
      RulesetMatcher::CreateVerifiedMatcher(source, result.ruleset_checksum,
                                            matcher);
  return load_result == RulesetMatcher::kLoadSuccess;
}

RulesetSource CreateTemporarySource(size_t id,
                                    dnr_api::SourceType source_type,
                                    size_t rule_count_limit,
                                    ExtensionId extension_id) {
  std::unique_ptr<RulesetSource> source = RulesetSource::CreateTemporarySource(
      id, source_type, rule_count_limit, std::move(extension_id));
  CHECK(source);
  return source->Clone();
}

dnr_api::ModifyHeaderInfo CreateModifyHeaderInfo(
    dnr_api::HeaderOperation operation,
    std::string header) {
  dnr_api::ModifyHeaderInfo header_info;

  header_info.operation = operation;
  header_info.header = header;

  return header_info;
}

bool EqualsForTesting(const dnr_api::ModifyHeaderInfo& lhs,
                      const dnr_api::ModifyHeaderInfo& rhs) {
  return lhs.operation == rhs.operation && lhs.header == rhs.header;
}

}  // namespace declarative_net_request
}  // namespace extensions
