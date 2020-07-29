// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_client.h"

#include <utility>

#include "chrome/browser/updates/update_notification_service.h"

namespace updates {

UpdateNotificationClient::UpdateNotificationClient(GetServiceCallback callback)
    : service_getter_(std::move(callback)) {}

UpdateNotificationClient::~UpdateNotificationClient() = default;

void UpdateNotificationClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  GetUpdateNotificationService()->BeforeShowNotification(
      std::move(notification_data), std::move(callback));
}

void UpdateNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guid) {
  NOTIMPLEMENTED();
}

void UpdateNotificationClient::OnUserAction(const UserActionData& action_data) {
  DCHECK(action_data.client_type ==
         notifications::SchedulerClientType::kChromeUpdate);
  if (action_data.action_type == notifications::UserActionType::kClick) {
    GetUpdateNotificationService()->OnUserClick(action_data.custom_data);
  }
}

void UpdateNotificationClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  GetUpdateNotificationService()->GetThrottleConfig(std::move(callback));
}

UpdateNotificationService*
UpdateNotificationClient::GetUpdateNotificationService() {
  return service_getter_.Run();
}

}  // namespace updates
