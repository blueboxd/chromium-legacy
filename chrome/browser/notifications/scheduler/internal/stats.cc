// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

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

void LogImpressionDbInit(bool success, int entry_count) {
  UMA_HISTOGRAM_BOOLEAN("Notifications.Scheduler.ImpressionDb.InitResult",
                        success);
  if (success) {
    UMA_HISTOGRAM_COUNTS_100("Notifications.Scheduler.ImpressionDb.RecordCount",
                             entry_count);
  }
}

void LogImpressionDbOperation(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Notifications.Scheduler.ImpressionDb.OperationResult",
                        success);
}

void LogImpressionCount(int impression_count, SchedulerClientType type) {
  std::string name("Notifications.Scheduler.Impression.Count.");
  name.append(ToHistogramSuffix(type));
  base::UmaHistogramCounts100(name, impression_count);
}

void LogImpressionrEvent(ImpressionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Scheduler.Impression.Event", event);
}

void LogNotificationDbInit(bool success, int entry_count) {
  UMA_HISTOGRAM_BOOLEAN("Notifications.Scheduler.NotificationDb.InitResult",
                        success);
  if (success) {
    UMA_HISTOGRAM_COUNTS_100(
        "Notifications.Scheduler.NotificationDb.RecordCount", entry_count);
  }
}

void LogNotificationDbOperation(bool success) {
  UMA_HISTOGRAM_BOOLEAN(
      "Notifications.Scheduler.NotificationDb.OperationResult", success);
}

}  // namespace stats
}  // namespace notifications
