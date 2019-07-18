// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace declarative_net_request {

// Per extension instance which manages the different rulesets for an extension
// while respecting their priorities.
class CompositeMatcher {
 public:
  struct RedirectAction {
    RedirectAction(base::Optional<GURL> redirect_url, bool notify);
    ~RedirectAction();
    RedirectAction(RedirectAction&& other);
    RedirectAction& operator=(RedirectAction&& other);

    // The URL the request will be redirected to. The request should not be
    // redirected if this is not specified.
    base::Optional<GURL> redirect_url;

    // Whether the extension should be notified that the request was unable to
    // be redirected as the extension lacks the appropriate host permission for
    // the request.
    bool notify_request_withheld = false;

    DISALLOW_COPY_AND_ASSIGN(RedirectAction);
  };

  using MatcherList = std::vector<std::unique_ptr<RulesetMatcher>>;

  // Each RulesetMatcher should have a distinct ID and priority.
  explicit CompositeMatcher(MatcherList matchers);
  ~CompositeMatcher();

  // Adds the |new_matcher| to the list of matchers. If a matcher with the
  // corresponding ID is already present, updates the matcher.
  void AddOrUpdateRuleset(std::unique_ptr<RulesetMatcher> new_matcher);

  // Returns whether the network request as specified by |params| should be
  // blocked.
  bool ShouldBlockRequest(const RequestParams& params) const;

  // Returns a RedirectAction struct containing a redirect URL if the request
  // is to be redirected, and whether the extension should be notified if its
  // access to the request is withheld.
  RedirectAction ShouldRedirectRequest(
      const RequestParams& params,
      PermissionsData::PageAccess page_access) const;

  // Returns the bitmask of headers to remove from the request. The bitmask
  // corresponds to RemoveHeadersMask type. |current_mask| denotes the current
  // mask of headers to be removed and is included in the return value.
  uint8_t GetRemoveHeadersMask(const RequestParams& params,
                               uint8_t current_mask) const;

  // Returns whether this modifies "extraHeaders".
  bool HasAnyExtraHeadersMatcher() const;

 private:
  bool ComputeHasAnyExtraHeadersMatcher() const;

  // Sorts |matchers_| in descending order of priority.
  void SortMatchersByPriority();

  // Sorted by priority in descending order.
  MatcherList matchers_;

  // Denotes the cached return value for |HasAnyExtraHeadersMatcher|. Care must
  // be taken to reset this as this object is modified.
  mutable base::Optional<bool> has_any_extra_headers_matcher_;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
