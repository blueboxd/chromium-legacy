// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_info.h"

namespace updates {

UpdateNotificationInfo::UpdateNotificationInfo() = default;

UpdateNotificationInfo::UpdateNotificationInfo(
    const UpdateNotificationInfo& other) = default;

bool UpdateNotificationInfo::operator==(
    const UpdateNotificationInfo& other) const {
  return title == other.title && message == other.message;
}

UpdateNotificationInfo::~UpdateNotificationInfo() = default;

}  // namespace updates
