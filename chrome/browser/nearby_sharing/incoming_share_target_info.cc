// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/incoming_share_target_info.h"

#include "chrome/browser/nearby_sharing/nearby_connection.h"

IncomingShareTargetInfo::IncomingShareTargetInfo() = default;
IncomingShareTargetInfo::~IncomingShareTargetInfo() = default;

IncomingShareTargetInfo::IncomingShareTargetInfo(IncomingShareTargetInfo&&) =
    default;
IncomingShareTargetInfo& IncomingShareTargetInfo::operator=(
    IncomingShareTargetInfo&&) = default;

std::ostream& operator<<(std::ostream& out,
                         const IncomingShareTargetInfo& share_target) {
  out << "IncomingShareTargetInfo<endpoint_id: "
      << (share_target.endpoint_id().has_value()
              ? share_target.endpoint_id().value()
              : "")
      << ", has_certificate: " << (share_target.certificate().has_value())
      << ", has_connection: " << (share_target.nearby_connection() != nullptr)
      << ">";
  return out;
}
