// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_METRICS_INTENT_HANDLING_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_APPS_METRICS_INTENT_HANDLING_METRICS_H_

#include <string>
#include <utility>

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/arc/metrics/arc_metrics_constants.h"

namespace apps {

class IntentHandlingMetrics {
 public:
  IntentHandlingMetrics();
  static void RecordIntentPickerMetrics(
      Source source,
      bool should_persist,
      AppsNavigationThrottle::PickerAction action,
      AppsNavigationThrottle::Platform platform);

  static void RecordIntentPickerUserInteractionMetrics(
      const std::string& selected_app_package,
      PickerEntryType entry_type,
      IntentPickerCloseReason close_reason,
      Source source,
      bool should_persist);
};

}  // namespace apps

#endif  // CHROME_BROWSER_CHROMEOS_APPS_METRICS_INTENT_HANDLING_METRICS_H_
