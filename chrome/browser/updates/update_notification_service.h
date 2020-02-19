// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace updates {

struct UpdateNotificationInfo;

// Service to schedule update notification via
// notifications::NotificationScheduleService.
class UpdateNotificationService : public KeyedService {
 public:
  using ExtraData = std::map<std::string, std::string>;

  // Try yo schedule an update notification.
  virtual void Schedule(UpdateNotificationInfo data) = 0;

  // Validate the notification is ready to show.
  virtual bool IsReadyToDisplay() const = 0;

  // Called when the notification is dismissed by user.
  virtual void OnUserDismiss() = 0;

  // Called when the notification is clicked by user. Passing |extra| for
  // processing custom data.
  virtual void OnUserClick(const ExtraData& extra) = 0;

  // Called when the helpful/unhelpful buttons are clicked.
  virtual void OnUserClickButton(bool is_positive_button) = 0;

  ~UpdateNotificationService() override = default;

 protected:
  UpdateNotificationService() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationService);
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_
