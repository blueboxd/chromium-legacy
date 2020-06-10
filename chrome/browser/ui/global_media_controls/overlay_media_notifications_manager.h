// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_H_

#include <string>

class OverlayMediaNotificationsManager {
 public:
  // Called by the OverlayMediaNotification when the widget has closed.
  virtual void OnOverlayNotificationClosed(const std::string& id) = 0;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_H_
