// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
struct WebRequestInfo;

namespace declarative_net_request {
class RulesetSource;

namespace flat {
struct ExtensionIndexedRuleset;
struct UrlRuleMetadata;
}  // namespace flat

class RulesetMatcher;

// Struct to hold parameters for a network request.
struct RequestParams {
  // |info| must outlive this instance.
  explicit RequestParams(const WebRequestInfo& info);
  RequestParams();
  ~RequestParams();

  // This is a pointer to a GURL. Hence the GURL must outlive this struct.
  const GURL* url = nullptr;
  url::Origin first_party_origin;
  url_pattern_index::flat::ElementType element_type =
      url_pattern_index::flat::ElementType_OTHER;
  bool is_third_party = false;

  // A map of RulesetMatchers to results of |HasMatchingAllowRule| for this
  // request. Used as a cache to prevent extra calls to |HasMatchingAllowRule|.
  mutable base::flat_map<const RulesetMatcher*, bool> allow_rule_cache;

  DISALLOW_COPY_AND_ASSIGN(RequestParams);
};

// RulesetMatcher encapsulates the Declarative Net Request API ruleset
// corresponding to a single RulesetSource. This uses the url_pattern_index
// component to achieve fast matching of network requests against declarative
// rules. Since this class is immutable, it is thread-safe. In practice it is
// accessed on the IO thread but created on a sequence where file IO is allowed.
class RulesetMatcher {
 public:
  // Describes the result of creating a RulesetMatcher instance.
  // This is logged as part of UMA. Hence existing values should not be re-
  // numbered or deleted. New values should be added before kLoadRulesetMax.
  enum LoadRulesetResult {
    // Ruleset loading succeeded.
    kLoadSuccess = 0,

    // Ruleset loading failed since the provided path did not exist.
    kLoadErrorInvalidPath = 1,

    // Ruleset loading failed due to a file read error.
    kLoadErrorFileRead = 2,

    // Ruleset loading failed due to a checksum mismatch.
    kLoadErrorChecksumMismatch = 3,

    // Ruleset loading failed due to version header mismatch.
    // TODO(karandeepb): This should be split into two cases:
    //    - When the indexed ruleset doesn't have the version header in the
    //      correct format.
    //    - When the indexed ruleset's version is not the same as that used by
    //      Chrome.
    kLoadErrorVersionMismatch = 4,

    kLoadResultMax
  };

  // Factory function to create a verified RulesetMatcher for |source|. Must be
  // called on a sequence where file IO is allowed. Returns kLoadSuccess on
  // success along with the ruleset |matcher|.
  static LoadRulesetResult CreateVerifiedMatcher(
      const RulesetSource& source,
      int expected_ruleset_checksum,
      std::unique_ptr<RulesetMatcher>* matcher);

  ~RulesetMatcher();

  // Returns whether the ruleset has a matching blocking rule.
  bool HasMatchingBlockRule(const RequestParams& params) const {
    return GetMatchingRule(params, flat::ActionIndex_block);
  }

  // Returns whether the ruleset has a matching allow rule.
  bool HasMatchingAllowRule(const RequestParams& params) const {
    return GetMatchingRule(params, flat::ActionIndex_allow);
  }

  // Returns the bitmask of headers to remove from the request. The bitmask
  // corresponds to RemoveHeadersMask type. |current_mask| denotes the current
  // mask of headers to be removed and is included in the return value.
  uint8_t GetRemoveHeadersMask(const RequestParams& params,
                               uint8_t current_mask) const;

  // Returns the ruleset's matching redirect rule and populates
  // |redirect_url| if there is a matching redirect rule, otherwise returns
  // nullptr.
  const url_pattern_index::flat::UrlRule* GetRedirectRule(
      const RequestParams& params,
      GURL* redirect_url) const;

  // Returns the ruleset's matching upgrade scheme rule or nullptr if no
  // matching rule is found or if the request's scheme is not upgradeable.
  const url_pattern_index::flat::UrlRule* GetUpgradeRule(
      const RequestParams& params) const;

  // Returns whether this modifies "extraHeaders".
  bool IsExtraHeadersMatcher() const { return is_extra_headers_matcher_; }

  // ID of the ruleset. Each extension can have multiple rulesets with
  // their own unique ids.
  size_t id() const { return id_; }

  // Priority of the ruleset. Each extension can have multiple rulesets with
  // their own different priorities.
  size_t priority() const { return priority_; }

 private:
  using UrlPatternIndexMatcher = url_pattern_index::UrlPatternIndexMatcher;
  using ExtensionMetadataList =
      flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>;

  explicit RulesetMatcher(std::string ruleset_data, size_t id, size_t priority);

  const url_pattern_index::flat::UrlRule* GetMatchingRule(
      const RequestParams& params,
      flat::ActionIndex index,
      UrlPatternIndexMatcher::FindRuleStrategy strategy =
          UrlPatternIndexMatcher::FindRuleStrategy::kAny) const;

  const std::string ruleset_data_;

  const flat::ExtensionIndexedRuleset* const root_;

  // UrlPatternIndexMatchers corresponding to entries in flat::ActionIndex.
  const std::vector<UrlPatternIndexMatcher> matchers_;

  const ExtensionMetadataList* const metadata_list_;

  size_t id_;
  size_t priority_;

  const bool is_extra_headers_matcher_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_
