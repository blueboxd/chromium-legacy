// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_

#include <vector>

#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher_base.h"

namespace extensions {
namespace declarative_net_request {

// ExtensionUrlPatternIndexMatcher is an implementation detail of
// RulesetMatcher. It deals with matching of filter list style rules. This uses
// the url_pattern_index component to achieve fast matching of network requests
// against declarative rules.
class ExtensionUrlPatternIndexMatcher final : public RulesetMatcherBase {
 public:
  using UrlPatternIndexList = flatbuffers::Vector<
      flatbuffers::Offset<url_pattern_index::flat::UrlPatternIndex>>;
  ExtensionUrlPatternIndexMatcher(
      const ExtensionId& extension_id,
      api::declarative_net_request::SourceType source_type,
      const UrlPatternIndexList* index_list,
      const ExtensionMetadataList* metadata_list);

  // RulesetMatcherBase override:
  ~ExtensionUrlPatternIndexMatcher() override;
  uint8_t GetRemoveHeadersMask(
      const RequestParams& params,
      uint8_t excluded_remove_headers_mask,
      std::vector<RequestAction>* remove_headers_actions) const override;
  bool IsExtraHeadersMatcher() const override {
    return is_extra_headers_matcher_;
  }

 private:
  using UrlPatternIndexMatcher = url_pattern_index::UrlPatternIndexMatcher;

  // RulesetMatcherBase override:
  base::Optional<RequestAction> GetAllowAllRequestsAction(
      const RequestParams& params) const override;
  base::Optional<RequestAction> GetBeforeRequestActionIgnoringAncestors(
      const RequestParams& params) const override;

  // Returns the highest priority action from
  // |flat::IndexType_before_request_except_allow_all_requests| index.
  base::Optional<RequestAction> GetBeforeRequestActionHelper(
      const RequestParams& params) const;

  const url_pattern_index::flat::UrlRule* GetMatchingRule(
      const RequestParams& params,
      flat::IndexType index,
      UrlPatternIndexMatcher::FindRuleStrategy strategy =
          UrlPatternIndexMatcher::FindRuleStrategy::kAny) const;

  const ExtensionMetadataList* const metadata_list_;

  // UrlPatternIndexMatchers corresponding to entries in flat::IndexType.
  const std::vector<UrlPatternIndexMatcher> matchers_;

  const bool is_extra_headers_matcher_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUrlPatternIndexMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_
