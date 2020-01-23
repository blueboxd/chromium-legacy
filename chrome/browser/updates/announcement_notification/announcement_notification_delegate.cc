// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"

#include "base/logging.h"

AnnouncementNotificationDelegate::AnnouncementNotificationDelegate() = default;

AnnouncementNotificationDelegate::~AnnouncementNotificationDelegate() = default;

void AnnouncementNotificationDelegate::ShowNotification() {
  NOTIMPLEMENTED();
}

bool AnnouncementNotificationDelegate::IsFirstRun() {
  NOTIMPLEMENTED();
  return false;
}
