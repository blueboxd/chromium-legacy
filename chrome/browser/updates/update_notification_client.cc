// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_client.h"

#include <utility>

#include "chrome/browser/updates/update_notification_service.h"

namespace updates {

UpdateNotificationClient::UpdateNotificationClient(GetServiceCallback callback)
    : get_service_callback_(std::move(callback)) {}

UpdateNotificationClient::~UpdateNotificationClient() = default;

void UpdateNotificationClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  auto* update_notification_service = get_service_callback_.Run();
  DCHECK(update_notification_service);
  if (!update_notification_service->IsReadyToDisplay()) {
    std::move(callback).Run(nullptr);
    return;
  }
  // TODO(hesen): Record metrics, and add iHNR buttons.
  std::move(callback).Run(std::move(notification_data));
}

void UpdateNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guid) {
  NOTIMPLEMENTED();
}

void UpdateNotificationClient::OnUserAction(const UserActionData& action_data) {
  DCHECK(action_data.client_type ==
         notifications::SchedulerClientType::kChromeUpdate);
  auto* update_notification_service = get_service_callback_.Run();
  DCHECK(update_notification_service);

  switch (action_data.action_type) {
    case notifications::UserActionType::kClick:
      update_notification_service->OnUserClick();
      break;
    case notifications::UserActionType::kButtonClick:
      NOTIMPLEMENTED();
      break;
    case notifications::UserActionType::kDismiss:
      update_notification_service->OnUserDismiss();
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace updates
