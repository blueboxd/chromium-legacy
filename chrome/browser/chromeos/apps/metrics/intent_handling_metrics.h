// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_METRICS_INTENT_HANDLING_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_APPS_METRICS_INTENT_HANDLING_METRICS_H_

#include <string>
#include <utility>

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"
#include "components/arc/metrics/arc_metrics_constants.h"

namespace apps {

class IntentHandlingMetrics {
 public:
  // The type of app the link came from, used for intent handling metrics.
  // This enum is used for recording histograms, and must be treated as
  // append-only.
  enum class AppType {
    kArc = 0,  // From an Android app
    kWeb,      // From a web app
    kMaxValue = kWeb,
  };

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

  static void RecordExternalProtocolMetrics(arc::Scheme scheme,
                                            apps::PickerEntryType entry_type,
                                            bool accepted,
                                            bool persisted);

  static void RecordOpenBrowserMetrics(AppType type);
};

}  // namespace apps

#endif  // CHROME_BROWSER_CHROMEOS_APPS_METRICS_INTENT_HANDLING_METRICS_H_
