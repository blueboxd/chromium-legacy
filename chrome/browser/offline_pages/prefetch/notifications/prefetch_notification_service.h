// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace offline_pages {

// Service to manage offline prefetch notifications via
// notifications::NotificationScheduleService.
class PrefetchNotificationService : public KeyedService {
 public:
  ~PrefetchNotificationService() override = default;

 protected:
  PrefetchNotificationService() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefetchNotificationService);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_H_
