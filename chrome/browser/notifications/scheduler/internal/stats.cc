// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/stats.h"

#include "base/metrics/histogram_functions.h"

namespace notifications {
namespace stats {
namespace {

// Returns the histogram suffix for a client type. Should match suffix
// NotificationSchedulerClientType in histograms.xml.
std::string ToHistogramSuffix(SchedulerClientType client_type) {
  switch (client_type) {
    case SchedulerClientType::kTest1:
    case SchedulerClientType::kTest2:
    case SchedulerClientType::kTest3:
      return "__Test__";
    case SchedulerClientType::kUnknown:
      return "Unknown";
    case SchedulerClientType::kWebUI:
      return "WebUI";
  }
}

}  // namespace

void LogUserAction(const UserActionData& action_data) {
  std::string name("Notifications.Scheduler.UserAction");
  base::UmaHistogramEnumeration(name, action_data.action_type);
  name.append(".").append(ToHistogramSuffix(action_data.client_type));
  base::UmaHistogramEnumeration(name, action_data.action_type);
}

}  // namespace stats
}  // namespace notifications
