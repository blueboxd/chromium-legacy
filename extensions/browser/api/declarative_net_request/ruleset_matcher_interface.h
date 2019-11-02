// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_INTERFACE_H_

#include <vector>

#include "base/optional.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace declarative_net_request {
struct RequestAction;
struct RequestParams;

// An abstract interface for rule matchers. Overridden by different kinds of
// matchers, e.g. filter lists and regex.
class RulesetMatcherInterface {
 public:
  virtual ~RulesetMatcherInterface() = default;

  // Returns the ruleset's matching RequestAction with type |BLOCK| or
  // |COLLAPSE|, or base::nullopt if the ruleset has no matching blocking rule.
  virtual base::Optional<RequestAction> GetBlockOrCollapseAction(
      const RequestParams& params) const = 0;

  // Returns whether the ruleset has a matching allow rule.
  virtual bool HasMatchingAllowRule(const RequestParams& params) const = 0;

  // Returns a RequestAction constructed from the matching redirect rule with
  // the highest priority, or base::nullopt if no matching redirect rules are
  // found for this request.
  virtual base::Optional<RequestAction> GetRedirectAction(
      const RequestParams& params) const = 0;

  // Returns a RequestAction constructed from the matching upgrade rule with the
  // highest priority, or base::nullopt if no matching upgrade rules are found
  // for this request.
  virtual base::Optional<RequestAction> GetUpgradeAction(
      const RequestParams& params) const = 0;

  // Returns the bitmask of headers to remove from the request. The bitmask
  // corresponds to flat::RemoveHeaderType. |ignored_mask| denotes the mask of
  // headers to be skipped for evaluation and is excluded in the return value.
  virtual uint8_t GetRemoveHeadersMask(
      const RequestParams& params,
      uint8_t ignored_mask,
      std::vector<RequestAction>* remove_headers_actions) const = 0;

  // Returns whether this modifies "extraHeaders".
  virtual bool IsExtraHeadersMatcher() const = 0;

  // Returns the extension ID with which this matcher is associated.
  virtual const ExtensionId& extension_id() const = 0;

  // The source type of the matcher.
  virtual api::declarative_net_request::SourceType source_type() const = 0;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_INTERFACE_H_
