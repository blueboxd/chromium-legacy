// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_metrics.h"

#include "base/metrics/histogram_macros.h"

void RecordAnnouncementHistogram(AnnouncementNotificationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Announcement.Events", event);
}
