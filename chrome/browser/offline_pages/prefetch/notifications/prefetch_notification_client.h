// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_CLIENT_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace offline_pages {
namespace prefetch {

class PrefetchNotificationService;

// Client side code for offline prefetch notification, integrated with
// notification scheduler system.
class PrefetchNotificationClient
    : public notifications::NotificationSchedulerClient {
 public:
  using NotificationData = notifications::NotificationData;
  using UserActionData = notifications::UserActionData;
  using ThrottleConfig = notifications::ThrottleConfig;
  using GetServiceCallback =
      base::RepeatingCallback<PrefetchNotificationService*()>;

  explicit PrefetchNotificationClient(GetServiceCallback callback);
  ~PrefetchNotificationClient() override;

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override;
  void OnUserAction(const UserActionData& action_data) override;
  void GetThrottleConfig(ThrottleConfigCallback callback) override;

  PrefetchNotificationService* GetPrefetchNotificationService();

  GetServiceCallback prefetch_service_getter_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchNotificationClient);
};

}  // namespace prefetch
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_CLIENT_H_
