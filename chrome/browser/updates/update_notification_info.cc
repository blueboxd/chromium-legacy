// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_info.h"

namespace updates {

UpdateNotificationInfo::UpdateNotificationInfo()
    : state(0), should_show_immediately(false) {}

UpdateNotificationInfo::UpdateNotificationInfo(
    const UpdateNotificationInfo& other) = default;

bool UpdateNotificationInfo::operator==(
    const UpdateNotificationInfo& other) const {
  return title == other.title && message == other.message &&
         state == other.state &&
         should_show_immediately == other.should_show_immediately;
}

UpdateNotificationInfo::~UpdateNotificationInfo() = default;

}  // namespace updates
