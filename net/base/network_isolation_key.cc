// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_key.h"
#include "base/feature_list.h"
#include "net/base/features.h"

namespace net {

namespace {

std::string GetOriginDebugString(const base::Optional<url::Origin>& origin) {
  return origin ? origin->GetDebugString() : "null";
}

}  // namespace

NetworkIsolationKey::NetworkIsolationKey(
    const base::Optional<url::Origin>& top_frame_origin,
    const base::Optional<url::Origin>& initiating_frame_origin)
    : use_initiating_frame_origin_(base::FeatureList::IsEnabled(
          net::features::kAppendInitiatingFrameOriginToNetworkIsolationKey)),
      top_frame_origin_(top_frame_origin) {
  if (use_initiating_frame_origin_) {
    // TODO(crbug.com/950069): Move this above if once call sites are modified.
    DCHECK(initiating_frame_origin);
    initiating_frame_origin_ = initiating_frame_origin;
  }
}

NetworkIsolationKey::NetworkIsolationKey()
    : use_initiating_frame_origin_(base::FeatureList::IsEnabled(
          net::features::kAppendInitiatingFrameOriginToNetworkIsolationKey)) {}

NetworkIsolationKey::NetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey::~NetworkIsolationKey() = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    NetworkIsolationKey&& network_isolation_key) = default;

std::string NetworkIsolationKey::ToString() const {
  if (IsTransient())
    return "";

  return top_frame_origin_->Serialize() +
         (use_initiating_frame_origin_
              ? " " + initiating_frame_origin_->Serialize()
              : "");
}

std::string NetworkIsolationKey::ToDebugString() const {
  // The space-separated serialization of the top frame and initiating origins.
  std::string return_string = GetOriginDebugString(top_frame_origin_);
  if (use_initiating_frame_origin_) {
    return_string += " " + GetOriginDebugString(initiating_frame_origin_);
  }
  return return_string;
}

bool NetworkIsolationKey::IsFullyPopulated() const {
  DCHECK(!use_initiating_frame_origin_ || initiating_frame_origin_.has_value());
  return top_frame_origin_.has_value();
}

bool NetworkIsolationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;
  return top_frame_origin_->opaque() ||
         (use_initiating_frame_origin_ && initiating_frame_origin_->opaque());
}

bool NetworkIsolationKey::IsEmpty() const {
  return !top_frame_origin_.has_value();
}

}  // namespace net
