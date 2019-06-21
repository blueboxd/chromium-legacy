// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_DISPLAY_AGENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_DISPLAY_AGENT_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

struct DisplayData {
  NotificationData notification_data;
  SkBitmap icon;
};

// Does actual work to show notification in the UI surface.
class DisplayAgent {
 public:
  // Creates the default DisplayAgent.
  static std::unique_ptr<DisplayAgent> Create();

  // Shows the notification in UI.
  virtual void ShowNotification(std::unique_ptr<DisplayData> display_data) = 0;

  virtual ~DisplayAgent() = default;

 protected:
  DisplayAgent() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayAgent);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_DISPLAY_AGENT_H_
