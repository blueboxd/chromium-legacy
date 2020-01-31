// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/metrics/intent_handling_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/arc/metrics/arc_metrics_constants.h"

namespace apps {

IntentHandlingMetrics::IntentHandlingMetrics() {}

void IntentHandlingMetrics::RecordIntentPickerMetrics(
    Source source,
    bool should_persist,
    AppsNavigationThrottle::PickerAction action,
    AppsNavigationThrottle::Platform platform) {
  // TODO(crbug.com/985233) For now External Protocol Dialog is only querying
  // ARC apps.
  if (source == Source::kExternalProtocol) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.ExternalProtocolDialog", action);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerAction", action);

    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerDestinationPlatform",
                              platform);
  }
}

void IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
    const std::string& selected_app_package,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    Source source,
    bool should_persist) {
  if (entry_type == PickerEntryType::kArc &&
      (close_reason == IntentPickerCloseReason::PREFERRED_APP_FOUND ||
       close_reason == IntentPickerCloseReason::OPEN_APP)) {
    UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction",
                              arc::UserInteractionType::APP_STARTED_FROM_LINK);
  }
  AppsNavigationThrottle::PickerAction action =
      AppsNavigationThrottle::GetPickerAction(entry_type, close_reason,
                                              should_persist);
  AppsNavigationThrottle::Platform platform =
      AppsNavigationThrottle::GetDestinationPlatform(selected_app_package,
                                                     action);
  RecordIntentPickerMetrics(source, should_persist, action, platform);
}

void IntentHandlingMetrics::RecordExternalProtocolMetrics(
    arc::Scheme scheme,
    PickerEntryType entry_type,
    bool accepted,
    bool persisted) {
  arc::ProtocolAction action =
      arc::GetProtocolAction(scheme, entry_type, accepted, persisted);
  if (accepted) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.ExternalProtocolDialog.Accepted",
                              action);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.ExternalProtocolDialog.Rejected",
                              action);
  }
}

void IntentHandlingMetrics::RecordOpenBrowserMetrics(AppType type) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.OpenBrowser", type);
}

}  // namespace apps
