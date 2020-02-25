// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_client.h"

#include <utility>

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service.h"

namespace offline_pages {

PrefetchNotificationClient::PrefetchNotificationClient(
    GetServiceCallback callback)
    : get_service_callback_(std::move(callback)) {}

PrefetchNotificationClient::~PrefetchNotificationClient() = default;

void PrefetchNotificationClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  NOTIMPLEMENTED();
}

void PrefetchNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guid) {
  NOTIMPLEMENTED();
}

void PrefetchNotificationClient::OnUserAction(
    const UserActionData& action_data) {
  NOTIMPLEMENTED();
}

std::unique_ptr<notifications::ThrottleConfig>
PrefetchNotificationClient::GetThrottleConfig() {
  return nullptr;
}

}  // namespace offline_pages
