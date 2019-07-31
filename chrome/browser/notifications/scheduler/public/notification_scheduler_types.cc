// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

UserActionData::UserActionData(SchedulerClientType client_type,
                               UserActionType action_type,
                               const std::string& guid)
    : client_type(client_type), action_type(action_type), guid(guid) {}

UserActionData::UserActionData(const UserActionData& other) = default;

UserActionData::~UserActionData() = default;

}  // namespace notifications
