// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_METRICS_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_METRICS_H_

// Used to record histograms, must match AnnouncementNotificationEvent in
// enums.xml. Cannot reuse or delete values.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.announcement)
enum class AnnouncementNotificationEvent {
  // Starts to check whether to show announcement notification.
  kStart = 0,
  // Notification is shown.
  kShown = 1,
  // Notification is clicked.
  kClick = 2,
  // Notification is closed.
  kClose = 3,
  // The acknowledge button is clicked.
  kAck = 4,
  // The open button is clicked.
  kOpen = 5,
  kMaxValue = kOpen,
};

// Records announcement notification event.
void RecordAnnouncementHistogram(AnnouncementNotificationEvent event);

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_METRICS_H_
