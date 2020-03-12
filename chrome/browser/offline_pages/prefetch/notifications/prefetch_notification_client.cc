// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_client.h"

#include <utility>

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service.h"

namespace offline_pages {
namespace prefetch {

PrefetchNotificationClient::PrefetchNotificationClient(
    GetServiceCallback callback)
    : prefetch_service_getter_(std::move(callback)) {}

PrefetchNotificationClient::~PrefetchNotificationClient() = default;

void PrefetchNotificationClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  std::move(callback).Run(std::move(notification_data));
}

void PrefetchNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guid) {
  NOTIMPLEMENTED();
}

void PrefetchNotificationClient::OnUserAction(
    const UserActionData& action_data) {
  if (action_data.action_type == notifications::UserActionType::kClick) {
    GetPrefetchNotificationService()->OnClick();
  }
}

void PrefetchNotificationClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  GetPrefetchNotificationService()->GetThrottleConfig(std::move(callback));
}

PrefetchNotificationService*
PrefetchNotificationClient::GetPrefetchNotificationService() {
  return prefetch_service_getter_.Run();
}

}  // namespace prefetch
}  // namespace offline_pages
