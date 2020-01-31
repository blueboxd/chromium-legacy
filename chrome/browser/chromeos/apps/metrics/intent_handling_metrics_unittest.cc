// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/metrics/intent_handling_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

typedef testing::Test IntentHandlingMetricsTest;

TEST_F(IntentHandlingMetricsTest, TestRecordIntentPickerMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test = IntentHandlingMetrics();
  test.RecordIntentPickerMetrics(
      Source::kExternalProtocol, false,
      AppsNavigationThrottle::PickerAction::ARC_APP_PREFERRED_PRESSED,
      AppsNavigationThrottle::Platform::ARC);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.ExternalProtocolDialog",
      AppsNavigationThrottle::PickerAction::ARC_APP_PREFERRED_PRESSED, 1);

  test.RecordIntentPickerMetrics(
      Source::kHttpOrHttps, true,
      AppsNavigationThrottle::PickerAction::ARC_APP_PREFERRED_PRESSED,
      AppsNavigationThrottle::Platform::ARC);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.IntentPickerAction",
      AppsNavigationThrottle::PickerAction::ARC_APP_PREFERRED_PRESSED, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.IntentPickerDestinationPlatform",
      AppsNavigationThrottle::Platform::ARC, 1);
}

TEST_F(IntentHandlingMetricsTest,
       TestRecordIntentPickerUserInteractionMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test = IntentHandlingMetrics();
  test.RecordIntentPickerUserInteractionMetrics(
      "app_package", PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP,
      Source::kExternalProtocol, true);

  histogram_tester.ExpectBucketCount(
      "Arc.UserInteraction", arc::UserInteractionType::APP_STARTED_FROM_LINK,
      1);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.ExternalProtocolDialog",
      AppsNavigationThrottle::PickerAction::ARC_APP_PREFERRED_PRESSED, 1);
}

TEST_F(IntentHandlingMetricsTest, TestRecordExternalProtocolMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test = IntentHandlingMetrics();
  test.RecordExternalProtocolMetrics(arc::Scheme::IRC, PickerEntryType::kArc,
                                     /*accepted=*/true, /*persisted=*/true);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.ExternalProtocolDialog.Accepted",
      arc::ProtocolAction::IRC_ACCEPTED_PERSISTED, 1);
}

TEST_F(IntentHandlingMetricsTest, TestRecordOpenBrowserMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test;
  test.RecordOpenBrowserMetrics(IntentHandlingMetrics::AppType::kArc);

  histogram_tester.ExpectBucketCount("ChromeOS.Apps.OpenBrowser",
                                     IntentHandlingMetrics::AppType::kArc, 1);
}

}  // namespace apps
