// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/outgoing_share_target_info.h"
#include "chrome/browser/nearby_sharing/nearby_connection.h"

OutgoingShareTargetInfo::OutgoingShareTargetInfo() = default;
OutgoingShareTargetInfo::~OutgoingShareTargetInfo() = default;

OutgoingShareTargetInfo::OutgoingShareTargetInfo(OutgoingShareTargetInfo&&) =
    default;
OutgoingShareTargetInfo& OutgoingShareTargetInfo::operator=(
    OutgoingShareTargetInfo&&) = default;

std::ostream& operator<<(std::ostream& out,
                         const OutgoingShareTargetInfo& share_target) {
  out << "OutgoingShareTargetInfo<endpoint_id: "
      << (share_target.endpoint_id().has_value()
              ? share_target.endpoint_id().value()
              : "")
      << ", has_certificate: " << (share_target.certificate().has_value())
      << ", has_connection: " << (share_target.connection() != nullptr) << ">";
  return out;
}
