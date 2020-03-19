// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/extension_url_pattern_index_matcher.h"

#include <algorithm>
#include <limits>
#include <list>
#include <string>
#include <utility>

#include "base/logging.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/common/api/declarative_net_request.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

std::vector<url_pattern_index::UrlPatternIndexMatcher> GetMatchers(
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list) {
  DCHECK(index_list);
  DCHECK_EQ(flat::IndexType_count, index_list->size());

  std::vector<url_pattern_index::UrlPatternIndexMatcher> matchers;
  matchers.reserve(flat::IndexType_count);
  for (const flat_rule::UrlPatternIndex* index : *index_list)
    matchers.emplace_back(index);
  return matchers;
}

bool HasAnyRules(const flat_rule::UrlPatternIndex* index) {
  DCHECK(index);

  if (index->fallback_rules()->size() > 0)
    return true;

  // Iterate over all ngrams and check their corresponding rules.
  for (auto* ngram_to_rules : *index->ngram_index()) {
    if (ngram_to_rules == index->ngram_index_empty_slot())
      continue;

    if (ngram_to_rules->rule_list()->size() > 0)
      return true;
  }

  return false;
}

bool IsExtraHeadersMatcherInternal(
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list) {
  // We only support removing a subset of extra headers currently. If that
  // changes, the implementation here should change as well.
  // TODO(crbug.com/947591): Modify this method for
  // flat::IndexType_modify_headers.
  static_assert(flat::IndexType_count == 6,
                "Modify this method to ensure IsExtraHeadersMatcherInternal is "
                "updated as new actions are added.");
  static const flat::IndexType extra_header_indices[] = {
      flat::IndexType_remove_cookie_header,
      flat::IndexType_remove_referer_header,
      flat::IndexType_remove_set_cookie_header,
  };

  for (flat::IndexType index : extra_header_indices) {
    if (HasAnyRules(index_list->Get(index)))
      return true;
  }

  return false;
}

}  // namespace

ExtensionUrlPatternIndexMatcher::ExtensionUrlPatternIndexMatcher(
    const ExtensionId& extension_id,
    api::declarative_net_request::SourceType source_type,
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list,
    const ExtensionMetadataList* metadata_list)
    : RulesetMatcherBase(extension_id, source_type),
      metadata_list_(metadata_list),
      matchers_(GetMatchers(index_list)),
      is_extra_headers_matcher_(IsExtraHeadersMatcherInternal(index_list)) {}

ExtensionUrlPatternIndexMatcher::~ExtensionUrlPatternIndexMatcher() = default;

uint8_t ExtensionUrlPatternIndexMatcher::GetRemoveHeadersMask(
    const RequestParams& params,
    uint8_t excluded_remove_headers_mask,
    std::vector<RequestAction>* remove_headers_actions) const {
  // The same flat_rule::UrlRule may be split across different action indices.
  // To ensure we return one RequestAction for one ID/rule, maintain a map from
  // the rule to the mask of rules removed for that rule.
  base::flat_map<const flat_rule::UrlRule*, uint8_t> rule_to_mask_map;
  auto handle_remove_header_bit = [this, &params, excluded_remove_headers_mask,
                                   &rule_to_mask_map](uint8_t bit,
                                                      flat::IndexType index) {
    if (excluded_remove_headers_mask & bit)
      return;

    const flat_rule::UrlRule* rule = GetMatchingRule(params, index);
    if (!rule)
      return;

    rule_to_mask_map[rule] |= bit;
  };

  // Iterate over each RemoveHeaderType value.
  uint8_t bit = 0;
  for (int i = 0; i <= dnr_api::REMOVE_HEADER_TYPE_LAST; ++i) {
    switch (i) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        bit = flat::RemoveHeaderType_cookie;
        handle_remove_header_bit(bit, flat::IndexType_remove_cookie_header);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        bit = flat::RemoveHeaderType_referer;
        handle_remove_header_bit(bit, flat::IndexType_remove_referer_header);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        bit = flat::RemoveHeaderType_set_cookie;
        handle_remove_header_bit(bit, flat::IndexType_remove_set_cookie_header);
        break;
    }
  }

  uint8_t mask = 0;
  for (const auto& it : rule_to_mask_map) {
    uint8_t mask_for_rule = it.second;
    DCHECK(mask_for_rule);
    mask |= mask_for_rule;

    remove_headers_actions->push_back(
        GetRemoveHeadersActionForMask(*it.first, mask_for_rule));
  }

  DCHECK(!(mask & excluded_remove_headers_mask));
  return mask;
}

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetAllowAllRequestsAction(
    const RequestParams& params) const {
  const flat_rule::UrlRule* rule =
      GetMatchingRule(params, flat::IndexType_allow_all_requests,
                      FindRuleStrategy::kHighestPriority);
  if (!rule)
    return base::nullopt;

  return CreateAllowAllRequestsAction(params, *rule);
}

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetBeforeRequestActionIgnoringAncestors(
    const RequestParams& params) const {
  return GetMaxPriorityAction(GetBeforeRequestActionHelper(params),
                              GetAllowAllRequestsAction(params));
}

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetBeforeRequestActionHelper(
    const RequestParams& params) const {
  const flat_rule::UrlRule* rule = GetMatchingRule(
      params, flat::IndexType_before_request_except_allow_all_requests,
      FindRuleStrategy::kHighestPriority);
  if (!rule)
    return base::nullopt;

  const flat::UrlRuleMetadata* metadata =
      metadata_list_->LookupByKey(rule->id());
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule->id());
  switch (metadata->action()) {
    case flat::ActionType_block:
      return CreateBlockOrCollapseRequestAction(params, *rule);
    case flat::ActionType_allow:
      return CreateAllowAction(params, *rule);
    case flat::ActionType_redirect:
      return CreateRedirectActionFromMetadata(params, *rule, *metadata_list_);
    case flat::ActionType_upgrade_scheme:
      return CreateUpgradeAction(params, *rule);
    case flat::ActionType_allow_all_requests:
    case flat::ActionType_remove_headers:
    case flat::ActionType_modify_headers:
    case flat::ActionType_count:
      NOTREACHED();
  }

  return base::nullopt;
}

const flat_rule::UrlRule* ExtensionUrlPatternIndexMatcher::GetMatchingRule(
    const RequestParams& params,
    flat::IndexType index,
    FindRuleStrategy strategy) const {
  DCHECK_LT(index, flat::IndexType_count);
  DCHECK_GE(index, 0);
  DCHECK(params.url);

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool kDisableGenericRules = false;

  return matchers_[index].FindMatch(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.is_third_party,
      kDisableGenericRules, strategy);
}

}  // namespace declarative_net_request
}  // namespace extensions
