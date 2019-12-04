// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "components/keyed_service/core/keyed_service.h"

namespace updates {

// Service to schedule update notification via
// notifications::NotificationScheduleService.
class UpdateNotificationService : public KeyedService {
 public:
  // Try yo schedule an update notification.
  virtual void Schedule(notifications::NotificationData data) = 0;

  ~UpdateNotificationService() override = default;

 protected:
  UpdateNotificationService() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationService);
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_
