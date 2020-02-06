// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_IMPL_H_

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service.h"

#include <memory>

#include "base/memory/weak_ptr.h"

namespace notifications {
class NotificationScheduleService;
}  // namespace notifications

namespace offline_pages {

// Service to manage offline prefetch notifications via
// notifications::NotificationScheduleService.
class PrefetchNotificationServiceImpl : public PrefetchNotificationService {
 public:
  PrefetchNotificationServiceImpl(
      notifications::NotificationScheduleService* schedule_service);
  ~PrefetchNotificationServiceImpl() override;

 private:
  // Used to schedule notification to show in the future. Must outlive this
  // class.
  notifications::NotificationScheduleService* schedule_service_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchNotificationServiceImpl);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_IMPL_H_
