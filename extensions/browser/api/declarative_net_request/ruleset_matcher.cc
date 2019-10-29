// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

constexpr const char kSetCookieResponseHeader[] = "set-cookie";

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

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

base::StringPiece CreateStringPiece(const ::flatbuffers::String& str) {
  return base::StringPiece(str.c_str(), str.size());
}

// Returns true if the given |vec| is nullptr or empty.
template <typename T>
bool IsEmpty(const flatbuffers::Vector<T>* vec) {
  return !vec || vec->size() == 0;
}

// Performs any required query transformations on the |url|. Returns true if the
// query should be modified and populates |modified_query|.
bool GetModifiedQuery(const GURL& url,
                      const flat::UrlTransform& transform,
                      std::string* modified_query) {
  DCHECK(modified_query);

  // |remove_query_params| should always be sorted.
  DCHECK(
      IsEmpty(transform.remove_query_params()) ||
      std::is_sorted(transform.remove_query_params()->begin(),
                     transform.remove_query_params()->end(),
                     [](const flatbuffers::String* x1,
                        const flatbuffers::String* x2) { return *x1 < *x2; }));

  // Return early if there's nothing to modify.
  if (IsEmpty(transform.remove_query_params()) &&
      IsEmpty(transform.add_or_replace_query_params())) {
    return false;
  }

  std::vector<base::StringPiece> remove_query_params;
  if (!IsEmpty(transform.remove_query_params())) {
    remove_query_params.reserve(transform.remove_query_params()->size());
    for (const ::flatbuffers::String* str : *transform.remove_query_params())
      remove_query_params.push_back(CreateStringPiece(*str));
  }

  // We don't use a map from keys to vector of values to ensure the relative
  // order of different params specified by the extension is respected. We use a
  // std::list to support fast removal from middle of the list. Note that the
  // key value pairs should already be escaped.
  std::list<std::pair<base::StringPiece, base::StringPiece>>
      add_or_replace_query_params;
  if (!IsEmpty(transform.add_or_replace_query_params())) {
    for (const flat::QueryKeyValue* query_pair :
         *transform.add_or_replace_query_params()) {
      DCHECK(query_pair->key());
      DCHECK(query_pair->value());
      add_or_replace_query_params.emplace_back(
          CreateStringPiece(*query_pair->key()),
          CreateStringPiece(*query_pair->value()));
    }
  }

  std::vector<std::string> query_parts;

  auto create_query_part = [](base::StringPiece key, base::StringPiece value) {
    return base::StrCat({key, "=", value});
  };

  bool query_changed = false;
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    std::string key = it.GetKey();
    // Remove query param.
    if (std::binary_search(remove_query_params.begin(),
                           remove_query_params.end(), key)) {
      query_changed = true;
      continue;
    }

    auto replace_iterator = std::find_if(
        add_or_replace_query_params.begin(), add_or_replace_query_params.end(),
        [&key](const std::pair<base::StringPiece, base::StringPiece>& param) {
          return param.first == key;
        });

    // Nothing to do.
    if (replace_iterator == add_or_replace_query_params.end()) {
      query_parts.push_back(create_query_part(key, it.GetValue()));
      continue;
    }

    // Replace query param.
    query_changed = true;
    query_parts.push_back(create_query_part(key, replace_iterator->second));
    add_or_replace_query_params.erase(replace_iterator);
  }

  // Append any remaining query params.
  for (const auto& params : add_or_replace_query_params) {
    query_changed = true;
    query_parts.push_back(create_query_part(params.first, params.second));
  }

  if (!query_changed)
    return false;

  *modified_query = base::JoinString(query_parts, "&");
  return true;
}

GURL GetTransformedURL(const RequestParams& params,
                       const flat::UrlTransform& transform) {
  GURL::Replacements replacements;

  if (transform.scheme())
    replacements.SetSchemeStr(CreateStringPiece(*transform.scheme()));

  if (transform.host())
    replacements.SetHostStr(CreateStringPiece(*transform.host()));

  DCHECK(!(transform.clear_port() && transform.port()));
  if (transform.clear_port())
    replacements.ClearPort();
  else if (transform.port())
    replacements.SetPortStr(CreateStringPiece(*transform.port()));

  DCHECK(!(transform.clear_path() && transform.path()));
  if (transform.clear_path())
    replacements.ClearPath();
  else if (transform.path())
    replacements.SetPathStr(CreateStringPiece(*transform.path()));

  // |query| is defined outside the if conditions since url::Replacements does
  // not own the strings it uses.
  std::string query;
  if (transform.clear_query()) {
    replacements.ClearQuery();
  } else if (transform.query()) {
    replacements.SetQueryStr(CreateStringPiece(*transform.query()));
  } else if (GetModifiedQuery(*params.url, transform, &query)) {
    replacements.SetQueryStr(query);
  }

  DCHECK(!(transform.clear_fragment() && transform.fragment()));
  if (transform.clear_fragment())
    replacements.ClearRef();
  else if (transform.fragment())
    replacements.SetRefStr(CreateStringPiece(*transform.fragment()));

  if (transform.password())
    replacements.SetPasswordStr(CreateStringPiece(*transform.password()));

  if (transform.username())
    replacements.SetUsernameStr(CreateStringPiece(*transform.username()));

  return params.url->ReplaceComponents(replacements);
}

bool ShouldCollapseResourceType(flat_rule::ElementType type) {
  // TODO(crbug.com/848842): Add support for other element types like
  // OBJECT.
  return type == flat_rule::ElementType_IMAGE ||
         type == flat_rule::ElementType_SUBDOCUMENT;
}

// Upgrades the url's scheme to HTTPS.
GURL GetUpgradedUrl(const GURL& url) {
  DCHECK(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kFtpScheme));

  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  return url.ReplaceComponents(replacements);
}

// Populates the list of headers corresponding to |mask|.
RequestAction GetRemoveHeadersActionForMask(const ExtensionId& extension_id,
                                            uint8_t mask,
                                            int rule_id,
                                            dnr_api::SourceType source_type) {
  RequestAction action(RequestAction::Type::REMOVE_HEADERS, rule_id,
                       source_type, extension_id);

  for (int i = 0; mask && i <= dnr_api::REMOVE_HEADER_TYPE_LAST; ++i) {
    switch (i) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        if (mask & kRemoveHeadersMask_Cookie)
          action.request_headers_to_remove.push_back(
              net::HttpRequestHeaders::kCookie);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        if (mask & kRemoveHeadersMask_Referer)
          action.request_headers_to_remove.push_back(
              net::HttpRequestHeaders::kReferer);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        if (mask & kRemoveHeadersMask_SetCookie)
          action.response_headers_to_remove.push_back(kSetCookieResponseHeader);
        break;
    }
  }

  return action;
}

}  // namespace

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
      std::move(ruleset_data), source.id(), source.priority(), source.type(),
      source.extension_id()));
  return kLoadSuccess;
}

RulesetMatcher::~RulesetMatcher() = default;

base::Optional<RequestAction> RulesetMatcher::GetBlockOrCollapseAction(
    const RequestParams& params) const {
  const flat_rule::UrlRule* rule =
      GetMatchingRule(params, flat::ActionIndex_block);
  if (!rule)
    return base::nullopt;

  return ShouldCollapseResourceType(params.element_type)
             ? RequestAction(RequestAction::Type::COLLAPSE, rule->id(),
                             source_type_, extension_id_)
             : RequestAction(RequestAction::Type::BLOCK, rule->id(),
                             source_type_, extension_id_);
}

base::Optional<RequestAction> RulesetMatcher::GetRedirectAction(
    const RequestParams& params) const {
  GURL redirect_rule_url;
  const flat_rule::UrlRule* redirect_rule =
      GetRedirectRule(params, &redirect_rule_url);

  if (!redirect_rule)
    return base::nullopt;

  RequestAction redirect_action(RequestAction::Type::REDIRECT,
                                redirect_rule->id(), source_type_,
                                extension_id_);
  redirect_action.redirect_url = std::move(redirect_rule_url);

  return redirect_action;
}

base::Optional<RequestAction> RulesetMatcher::GetUpgradeAction(
    const RequestParams& params) const {
  const flat_rule::UrlRule* upgrade_rule = GetUpgradeRule(params);

  if (!upgrade_rule)
    return base::nullopt;

  RequestAction upgrade_action(RequestAction::Type::REDIRECT,
                               upgrade_rule->id(), source_type_, extension_id_);
  upgrade_action.redirect_url = GetUpgradedUrl(*params.url);

  return upgrade_action;
}

base::Optional<RequestAction>
RulesetMatcher::GetRedirectOrUpgradeActionByPriority(
    const RequestParams& params) const {
  GURL redirect_rule_url;
  int rule_id = kMinValidID - 1;
  const flat_rule::UrlRule* redirect_rule =
      GetRedirectRule(params, &redirect_rule_url);
  const flat_rule::UrlRule* upgrade_rule = GetUpgradeRule(params);

  if (!redirect_rule && !upgrade_rule)
    return base::nullopt;

  GURL highest_priority_url;
  if (!upgrade_rule) {
    highest_priority_url = std::move(redirect_rule_url);
    rule_id = redirect_rule->id();
  } else if (!redirect_rule) {
    rule_id = upgrade_rule->id();
    highest_priority_url = GetUpgradedUrl(*params.url);
  } else {
    highest_priority_url = upgrade_rule->priority() > redirect_rule->priority()
                               ? GetUpgradedUrl(*params.url)
                               : std::move(redirect_rule_url);

    rule_id = upgrade_rule->priority() > redirect_rule->priority()
                  ? upgrade_rule->id()
                  : redirect_rule->id();
  }

  // Check that the resultant |rule_id| is valid.
  DCHECK_GE(rule_id, kMinValidID);

  RequestAction action(RequestAction::Type::REDIRECT, rule_id, source_type_,
                       extension_id_);
  action.redirect_url = std::move(highest_priority_url);

  return action;
}

uint8_t RulesetMatcher::GetRemoveHeadersMask(
    const RequestParams& params,
    uint8_t ignored_mask,
    std::vector<RequestAction>* remove_headers_actions) const {
  uint8_t mask = 0;

  static_assert(kRemoveHeadersMask_Max <= std::numeric_limits<uint8_t>::max(),
                "RemoveHeadersMask can't fit in a uint8_t");

  // Iterate over each RemoveHeaderType value.
  uint8_t bit = 0;

  // Rules with the same IDs may be split across different action indices. To
  // ensure we return one RequestAction for one ID, maintain a map from the rule
  // ID to the mask of rules removed for that rule ID.
  base::flat_map<uint32_t, uint8_t> rule_id_to_mask;
  auto handle_remove_header_bit = [this, &params, ignored_mask,
                                   &rule_id_to_mask, &mask](
                                      uint8_t bit, flat::ActionIndex index) {
    if (ignored_mask & bit)
      return;

    const flat_rule::UrlRule* rule = GetMatchingRule(params, index);
    if (!rule)
      return;

    rule_id_to_mask[rule->id()] |= bit;
    mask |= bit;
  };

  for (int i = 0; i <= dnr_api::REMOVE_HEADER_TYPE_LAST; ++i) {
    switch (i) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        bit = kRemoveHeadersMask_Cookie;
        handle_remove_header_bit(bit, flat::ActionIndex_remove_cookie_header);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        bit = kRemoveHeadersMask_Referer;
        handle_remove_header_bit(bit, flat::ActionIndex_remove_referer_header);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        bit = kRemoveHeadersMask_SetCookie;
        handle_remove_header_bit(bit,
                                 flat::ActionIndex_remove_set_cookie_header);
        break;
    }
  }

  for (auto it = rule_id_to_mask.begin(); it != rule_id_to_mask.end(); ++it) {
    uint8_t mask_for_rule = it->second;
    DCHECK(mask_for_rule);

    remove_headers_actions->push_back(GetRemoveHeadersActionForMask(
        extension_id_, mask_for_rule, it->first /* rule_id */, source_type_));
  }

  DCHECK(!(mask & ignored_mask));
  return mask;
}

RulesetMatcher::RulesetMatcher(std::string ruleset_data,
                               size_t id,
                               size_t priority,
                               dnr_api::SourceType source_type,
                               const ExtensionId& extension_id)
    : ruleset_data_(std::move(ruleset_data)),
      root_(flat::GetExtensionIndexedRuleset(ruleset_data_.data())),
      matchers_(GetMatchers(root_)),
      metadata_list_(root_->extension_metadata()),
      id_(id),
      priority_(priority),
      source_type_(source_type),
      extension_id_(extension_id),
      is_extra_headers_matcher_(IsExtraHeadersMatcherInternal(*root_)) {}

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
  DCHECK(metadata->redirect_url() || metadata->transform());

  if (metadata->redirect_url())
    *redirect_url = GURL(CreateStringPiece(*metadata->redirect_url()));
  else
    *redirect_url = GetTransformedURL(params, *metadata->transform());

  // Sanity check that we don't redirect to a javascript url. Specifying
  // redirect to a javascript url and specifying javascript as a transform
  // scheme is prohibited. In addition extensions can't intercept requests to
  // javascript urls. Hence we should never end up with a javascript url here.
  DCHECK(!redirect_url->SchemeIs(url::kJavaScriptScheme));

  // Prevent a redirect loop where a URL continuously redirects to itself.
  return (redirect_url->is_valid() && *params.url != *redirect_url) ? rule
                                                                    : nullptr;
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
