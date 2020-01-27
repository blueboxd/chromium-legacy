// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_

#include <cstdint>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

// A struct representing an action to be applied to a request based on DNR rule
// matches. Each action is attributed to exactly one extension.
struct RequestAction {
  enum class Type {
    // Block the network request.
    BLOCK,
    // Block the network request and collapse the corresponding DOM element.
    COLLAPSE,
    // Allow the network request, preventing it from being intercepted by other
    // matching rules.
    ALLOW,
    // Redirect the network request.
    REDIRECT,
    // Upgrade the scheme of the network request.
    UPGRADE,
    // Remove request/response headers.
    REMOVE_HEADERS,
    // Allow the network request. This request is either for an allowlisted
    // frame or originated from one.
    ALLOW_ALL_REQUESTS,
  };

  RequestAction(Type type,
                uint32_t rule_id,
                uint64_t index_priority,
                api::declarative_net_request::SourceType source_type,
                const ExtensionId& extension_id);
  ~RequestAction();
  RequestAction(RequestAction&&);
  RequestAction& operator=(RequestAction&&);

  // Helper to create a copy.
  RequestAction Clone() const;

  Type type = Type::BLOCK;

  // Valid iff |IsRedirectOrUpgrade()| is true.
  base::Optional<GURL> redirect_url;

  // The ID of the matching rule for this action.
  uint32_t rule_id;

  // The priority of this action in the index. This is a combination of the
  // rule's priority and the rule's action's priority.
  uint64_t index_priority;

  // The source type of the matching rule for this action.
  api::declarative_net_request::SourceType source_type;

  // The id of the extension the action is attributed to.
  ExtensionId extension_id;

  // Valid iff |type| is |REMOVE_HEADERS|. The vectors point to strings of
  // static storage duration.
  std::vector<const char*> request_headers_to_remove;
  std::vector<const char*> response_headers_to_remove;

  // Whether the action has already been tracked by the ActionTracker.
  // TODO(crbug.com/983761): Move the tracking of actions matched to
  // ActionTracker.
  mutable bool tracked = false;

  bool IsBlockOrCollapse() const {
    return type == Type::BLOCK || type == Type::COLLAPSE;
  }
  bool IsRedirectOrUpgrade() const {
    return type == Type::REDIRECT || type == Type::UPGRADE;
  }
  bool IsAllowOrAllowAllRequests() const {
    return type == Type::ALLOW || type == Type::ALLOW_ALL_REQUESTS;
  }

 private:
  RequestAction(const RequestAction&);
};

base::Optional<RequestAction> GetMaxPriorityAction(
    base::Optional<RequestAction> lhs,
    base::Optional<RequestAction> rhs);

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_
