// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_IMPL_H_

#include "chrome/browser/updates/update_notification_service.h"

#include "chrome/browser/notifications/scheduler/public/client_overview.h"

namespace updates {

class UpdateNotificationServiceImpl : public UpdateNotificationService {
 public:
  UpdateNotificationServiceImpl();
  ~UpdateNotificationServiceImpl() override;

 private:
  // UpdateNotificationService implementation.
  void Schedule(notifications::NotificationData data) override;

  // Called after querying the |ClientOverview| struct from scheduler system
  // completed.
  void OnClientOverviewQueried(notifications::ClientOverview overview);

  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationServiceImpl);
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_IMPL_H_
