// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/android_metrics_provider.h"

#include "chrome/android/chrome_jni_headers/NotificationSystemStatusUtil_jni.h"

#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/locale/locale_manager.h"

namespace {

// Corresponds to APP_NOTIFICATIONS_STATUS_BOUNDARY in
// NotificationSystemStatusUtil.java
const int kAppNotificationStatusBoundary = 3;

void EmitLowRamDeviceHistogram() {
  // Equivalent to UMA_HISTOGRAM_BOOLEAN with the stability flag set.
  UMA_STABILITY_HISTOGRAM_ENUMERATION(
      "MemoryAndroid.LowRamDevice", base::SysInfo::IsLowEndDevice() ? 1 : 0, 2);
}

void EmitAppNotificationStatusHistogram() {
  auto status = Java_NotificationSystemStatusUtil_getAppNotificationStatus(
      base::android::AttachCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("Android.AppNotificationStatus", status,
                            kAppNotificationStatusBoundary);
}

}  // namespace

AndroidMetricsProvider::AndroidMetricsProvider() {}

AndroidMetricsProvider::~AndroidMetricsProvider() {
}

void AndroidMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // The low-ram device status is unlikely to change between browser restarts.
  // Hence, it's safe and useful to attach this status to a previous session
  // log.
  EmitLowRamDeviceHistogram();
}

void AndroidMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  EmitLowRamDeviceHistogram();
  UMA_HISTOGRAM_ENUMERATION(
      "CustomTabs.Visible",
      chrome::android::GetCustomTabsVisibleValue(),
      chrome::android::CUSTOM_TABS_VISIBILITY_MAX);
  UMA_HISTOGRAM_BOOLEAN(
      "Android.MultiWindowMode.Active",
      chrome::android::GetIsInMultiWindowModeValue());
  EmitAppNotificationStatusHistogram();
  LocaleManager::RecordUserTypeMetrics();
}
