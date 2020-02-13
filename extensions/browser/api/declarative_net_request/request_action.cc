// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/request_action.h"

namespace extensions {
namespace declarative_net_request {

RequestAction::RequestAction(
    RequestAction::Type type,
    uint32_t rule_id,
    uint64_t index_priority,
    api::declarative_net_request::SourceType source_type,
    const ExtensionId& extension_id)
    : type(type),
      rule_id(rule_id),
      index_priority(index_priority),
      source_type(source_type),
      extension_id(extension_id) {}
RequestAction::~RequestAction() = default;
RequestAction::RequestAction(RequestAction&&) = default;
RequestAction& RequestAction::operator=(RequestAction&&) = default;

RequestAction RequestAction::Clone() const {
  // Use the private copy constructor to create a copy.
  return *this;
}

RequestAction::RequestAction(const RequestAction&) = default;

base::Optional<RequestAction> GetMaxPriorityAction(
    base::Optional<RequestAction> lhs,
    base::Optional<RequestAction> rhs) {
  if (!lhs)
    return rhs;
  if (!rhs)
    return lhs;
  return lhs->index_priority >= rhs->index_priority ? std::move(lhs)
                                                    : std::move(rhs);
}

}  // namespace declarative_net_request
}  // namespace extensions
