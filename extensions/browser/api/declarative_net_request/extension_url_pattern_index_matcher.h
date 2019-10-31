// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_

#include <vector>

#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher_interface.h"

namespace extensions {
namespace declarative_net_request {

// ExtensionUrlPatternIndexMatcher is an implementation detail of
// RulesetMatcher. It deals with matching of filter list style rules. This uses
// the url_pattern_index component to achieve fast matching of network requests
// against declarative rules.
class ExtensionUrlPatternIndexMatcher final : public RulesetMatcherInterface {
 public:
  using UrlPatternIndexList = flatbuffers::Vector<
      flatbuffers::Offset<url_pattern_index::flat::UrlPatternIndex>>;
  using ExtensionMetadataList =
      flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>;
  ExtensionUrlPatternIndexMatcher(
      const ExtensionId& extension_id,
      api::declarative_net_request::SourceType source_type,
      const UrlPatternIndexList* index_list,
      const ExtensionMetadataList* metadata_list);

  // RulesetMatcherInterface override:
  ~ExtensionUrlPatternIndexMatcher() override;
  base::Optional<RequestAction> GetBlockOrCollapseAction(
      const RequestParams& params) const override;
  bool HasMatchingAllowRule(const RequestParams& params) const override;
  base::Optional<RequestAction> GetRedirectAction(
      const RequestParams& params) const override;
  base::Optional<RequestAction> GetUpgradeAction(
      const RequestParams& params) const override;
  uint8_t GetRemoveHeadersMask(
      const RequestParams& params,
      uint8_t ignored_mask,
      std::vector<RequestAction>* remove_headers_actions) const override;
  bool IsExtraHeadersMatcher() const override {
    return is_extra_headers_matcher_;
  }
  const ExtensionId& extension_id() const override { return extension_id_; }
  api::declarative_net_request::SourceType source_type() const override {
    return source_type_;
  }

 private:
  using UrlPatternIndexMatcher = url_pattern_index::UrlPatternIndexMatcher;

  // Returns the ruleset's highest priority matching redirect rule and populates
  // |redirect_url|. Returns nullptr if there is no matching redirect rule.
  const url_pattern_index::flat::UrlRule* GetRedirectRule(
      const RequestParams& params,
      GURL* redirect_url) const;

  // Returns the ruleset's highest priority matching upgrade scheme rule or
  // nullptr if no matching rule is found or if the request's scheme is not
  // upgradeable.
  const url_pattern_index::flat::UrlRule* GetUpgradeRule(
      const RequestParams& params) const;

  const url_pattern_index::flat::UrlRule* GetMatchingRule(
      const RequestParams& params,
      flat::ActionIndex index,
      UrlPatternIndexMatcher::FindRuleStrategy strategy =
          UrlPatternIndexMatcher::FindRuleStrategy::kAny) const;

  const ExtensionId extension_id_;
  const api::declarative_net_request::SourceType source_type_;

  const ExtensionMetadataList* const metadata_list_;

  // UrlPatternIndexMatchers corresponding to entries in flat::ActionIndex.
  const std::vector<UrlPatternIndexMatcher> matchers_;

  const bool is_extra_headers_matcher_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUrlPatternIndexMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_
