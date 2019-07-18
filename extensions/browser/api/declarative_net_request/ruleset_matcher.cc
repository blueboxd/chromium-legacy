// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/common/resource_type.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

// Maps content::ResourceType to flat_rule::ElementType.
flat_rule::ElementType GetElementType(content::ResourceType type) {
  switch (type) {
    case content::ResourceType::kPrefetch:
    case content::ResourceType::kSubResource:
      return flat_rule::ElementType_OTHER;
    case content::ResourceType::kMainFrame:
    case content::ResourceType::kNavigationPreloadMainFrame:
      return flat_rule::ElementType_MAIN_FRAME;
    case content::ResourceType::kCspReport:
      return flat_rule::ElementType_CSP_REPORT;
    case content::ResourceType::kScript:
    case content::ResourceType::kWorker:
    case content::ResourceType::kSharedWorker:
    case content::ResourceType::kServiceWorker:
      return flat_rule::ElementType_SCRIPT;
    case content::ResourceType::kImage:
    case content::ResourceType::kFavicon:
      return flat_rule::ElementType_IMAGE;
    case content::ResourceType::kStylesheet:
      return flat_rule::ElementType_STYLESHEET;
    case content::ResourceType::kObject:
    case content::ResourceType::kPluginResource:
      return flat_rule::ElementType_OBJECT;
    case content::ResourceType::kXhr:
      return flat_rule::ElementType_XMLHTTPREQUEST;
    case content::ResourceType::kSubFrame:
    case content::ResourceType::kNavigationPreloadSubFrame:
      return flat_rule::ElementType_SUBDOCUMENT;
    case content::ResourceType::kPing:
      return flat_rule::ElementType_PING;
    case content::ResourceType::kMedia:
      return flat_rule::ElementType_MEDIA;
    case content::ResourceType::kFontResource:
      return flat_rule::ElementType_FONT;
  }
  NOTREACHED();
  return flat_rule::ElementType_OTHER;
}

// Returns the flat_rule::ElementType for the given |request|.
flat_rule::ElementType GetElementType(const WebRequestInfo& request) {
  if (request.url.SchemeIsWSOrWSS())
    return flat_rule::ElementType_WEBSOCKET;

  return request.type.has_value() ? GetElementType(request.type.value())
                                  : flat_rule::ElementType_OTHER;
}

// Returns whether the request to |url| is third party to its |document_origin|.
// TODO(crbug.com/696822): Look into caching this.
bool IsThirdPartyRequest(const GURL& url, const url::Origin& document_origin) {
  if (document_origin.opaque())
    return true;

  return !net::registry_controlled_domains::SameDomainOrHost(
      url, document_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

std::vector<url_pattern_index::UrlPatternIndexMatcher> GetMatchers(
    const flat::ExtensionIndexedRuleset* root) {
  DCHECK(root);
  DCHECK(root->index_list());
  DCHECK_EQ(flat::ActionIndex_count, root->index_list()->size());

  std::vector<url_pattern_index::UrlPatternIndexMatcher> matchers;
  matchers.reserve(flat::ActionIndex_count);
  for (const flat_rule::UrlPatternIndex* index : *(root->index_list()))
    matchers.emplace_back(index);
  return matchers;
}

bool HasAnyRules(const url_pattern_index::flat::UrlPatternIndex* index) {
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
    const flat::ExtensionIndexedRuleset& ruleset) {
  // We only support removing a subset of extra headers currently. If that
  // changes, the implementation here should change as well.
  static_assert(flat::ActionIndex_count == 7,
                "Modify this method to ensure IsExtraHeadersMatcherInternal is "
                "updated as new actions are added.");
  static const flat::ActionIndex extra_header_indices[] = {
      flat::ActionIndex_remove_cookie_header,
      flat::ActionIndex_remove_referer_header,
      flat::ActionIndex_remove_set_cookie_header,
  };

  for (flat::ActionIndex index : extra_header_indices) {
    if (HasAnyRules(ruleset.index_list()->Get(index)))
      return true;
  }

  return false;
}

}  // namespace

RequestParams::RequestParams(const WebRequestInfo& info)
    : url(&info.url),
      first_party_origin(info.initiator.value_or(url::Origin())),
      element_type(GetElementType(info)) {
  is_third_party = IsThirdPartyRequest(*url, first_party_origin);
}

RequestParams::RequestParams() = default;
RequestParams::~RequestParams() = default;

// static
RulesetMatcher::LoadRulesetResult RulesetMatcher::CreateVerifiedMatcher(
    const RulesetSource& source,
    int expected_ruleset_checksum,
    std::unique_ptr<RulesetMatcher>* matcher) {
  DCHECK(matcher);
  DCHECK(IsAPIAvailable());

  base::ElapsedTimer timer;

  if (!base::PathExists(source.indexed_path()))
    return kLoadErrorInvalidPath;

  std::string ruleset_data;
  if (!base::ReadFileToString(source.indexed_path(), &ruleset_data))
    return kLoadErrorFileRead;

  if (!StripVersionHeaderAndParseVersion(&ruleset_data))
    return kLoadErrorVersionMismatch;

  // This guarantees that no memory access will end up outside the buffer.
  if (!IsValidRulesetData(
          base::make_span(reinterpret_cast<const uint8_t*>(ruleset_data.data()),
                          ruleset_data.size()),
          expected_ruleset_checksum)) {
    return kLoadErrorChecksumMismatch;
  }

  UMA_HISTOGRAM_TIMES(
      "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
      timer.Elapsed());

  // Using WrapUnique instead of make_unique since this class has a private
  // constructor.
  *matcher = base::WrapUnique(new RulesetMatcher(
      std::move(ruleset_data), source.id(), source.priority()));
  return kLoadSuccess;
}

RulesetMatcher::~RulesetMatcher() = default;

uint8_t RulesetMatcher::GetRemoveHeadersMask(const RequestParams& params,
                                             uint8_t current_mask) const {
  uint8_t mask = current_mask;

  static_assert(kRemoveHeadersMask_Max <= std::numeric_limits<uint8_t>::max(),
                "RemoveHeadersMask can't fit in a uint8_t");

  // Iterate over each RemoveHeaderType value.
  uint8_t bit = 0;
  for (int i = 0; i <= dnr_api::REMOVE_HEADER_TYPE_LAST; ++i) {
    switch (i) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        bit = kRemoveHeadersMask_Cookie;
        if (mask & bit)
          break;
        if (GetMatchingRule(params, flat::ActionIndex_remove_cookie_header))
          mask |= bit;
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        bit = kRemoveHeadersMask_Referer;
        if (mask & bit)
          break;
        if (GetMatchingRule(params, flat::ActionIndex_remove_referer_header))
          mask |= bit;
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        bit = kRemoveHeadersMask_SetCookie;
        if (mask & bit)
          break;
        if (GetMatchingRule(params, flat::ActionIndex_remove_set_cookie_header))
          mask |= bit;
        break;
    }
  }

  return mask;
}

const flat_rule::UrlRule* RulesetMatcher::GetRedirectRule(
    const RequestParams& params,
    GURL* redirect_url) const {
  DCHECK(redirect_url);
  DCHECK_NE(flat_rule::ElementType_WEBSOCKET, params.element_type);

  const flat_rule::UrlRule* rule = GetMatchingRule(
      params, flat::ActionIndex_redirect, FindRuleStrategy::kHighestPriority);
  if (!rule)
    return nullptr;

  // Find the UrlRuleMetadata corresponding to |rule|. Since |metadata_list_| is
  // sorted by rule id, use LookupByKey which binary searches for fast lookup.
  const flat::UrlRuleMetadata* metadata =
      metadata_list_->LookupByKey(rule->id());

  // There must be a UrlRuleMetadata object corresponding to each redirect rule.
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule->id());

  *redirect_url = GURL(base::StringPiece(metadata->redirect_url()->c_str(),
                                         metadata->redirect_url()->size()));
  DCHECK(redirect_url->is_valid());
  // Prevent a redirect loop where a URL continuously redirects to itself.
  return *params.url == *redirect_url ? nullptr : rule;
}

const flat_rule::UrlRule* RulesetMatcher::GetUpgradeRule(
    const RequestParams& params) const {
  const bool is_upgradeable = params.url->SchemeIs(url::kHttpScheme) ||
                              params.url->SchemeIs(url::kFtpScheme);
  return is_upgradeable
             ? GetMatchingRule(params, flat::ActionIndex_upgrade_scheme,
                               FindRuleStrategy::kHighestPriority)
             : nullptr;
}

RulesetMatcher::RulesetMatcher(std::string ruleset_data,
                               size_t id,
                               size_t priority)
    : ruleset_data_(std::move(ruleset_data)),
      root_(flat::GetExtensionIndexedRuleset(ruleset_data_.data())),
      matchers_(GetMatchers(root_)),
      metadata_list_(root_->extension_metadata()),
      id_(id),
      priority_(priority),
      is_extra_headers_matcher_(IsExtraHeadersMatcherInternal(*root_)) {}

const flat_rule::UrlRule* RulesetMatcher::GetMatchingRule(
    const RequestParams& params,
    flat::ActionIndex index,
    FindRuleStrategy strategy) const {
  DCHECK_LT(index, flat::ActionIndex_count);
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
