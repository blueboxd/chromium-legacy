// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

// Default delegate for AnnouncementNotificationService that works on
// non-Android platforms.
class AnnouncementNotificationDelegate
    : public AnnouncementNotificationService::Delegate {
 public:
  AnnouncementNotificationDelegate();
  ~AnnouncementNotificationDelegate() override;

 private:
  // AnnouncementNotificationService::Delegate implementation.
  void ShowNotification() override;
  bool IsFirstRun() override;

  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationDelegate);
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_
