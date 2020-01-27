// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_app_interface.h"

#include "components/browsing_data/core/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test specific helpers for settings_egtest.mm.
@implementation SettingsAppInterface : NSObject

+ (void)restoreClearBrowsingDataCheckmarksToDefault {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* preferences = browserState->GetPrefs();
  preferences->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
  preferences->SetBoolean(browsing_data::prefs::kDeleteCache, true);
  preferences->SetBoolean(browsing_data::prefs::kDeleteCookies, true);
  preferences->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
  preferences->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
}

+ (BOOL)isMetricsRecordingEnabled {
  return chrome_test_util::IsMetricsRecordingEnabled();
}

+ (BOOL)isMetricsReportingEnabled {
  return chrome_test_util::IsMetricsReportingEnabled();
}

+ (void)setMetricsReportingEnabled:(BOOL)reportingEnabled {
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, reportingEnabled);
  // Breakpad uses dispatch_async to update its state. Wait to get to a
  // consistent state.
  chrome_test_util::WaitForBreakpadQueue();
}

// TODO(crbug.com/953862): Remove as part of feature flag cleanup.
+ (void)setMetricsReportingEnabled:(BOOL)reportingEnabled
                          wifiOnly:(BOOL)wifiOnly {
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, reportingEnabled);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             wifiOnly);
  // Breakpad uses dispatch_async to update its state. Wait to get to a
  // consistent state.
  chrome_test_util::WaitForBreakpadQueue();
}

+ (void)setCellularNetworkEnabled:(BOOL)cellularNetworkEnabled {
  chrome_test_util::SetWWANStateTo(cellularNetworkEnabled);
  // Breakpad uses dispatch_async to update its state. Wait to get to a
  // consistent state.
  chrome_test_util::WaitForBreakpadQueue();
}

+ (BOOL)isBreakpadEnabled {
  return chrome_test_util::IsBreakpadEnabled();
}

+ (BOOL)isBreakpadReportingEnabled {
  return chrome_test_util::IsBreakpadReportingEnabled();
}

+ (void)resetFirstLaunchState {
  chrome_test_util::SetFirstLaunchStateTo(
      chrome_test_util::IsFirstLaunchAfterUpgrade());
}

+ (void)setFirstLunchState:(BOOL)firstLaunch {
  chrome_test_util::SetFirstLaunchStateTo(NO);
}

+ (BOOL)settingsRegisteredKeyboardCommands {
  UIViewController* viewController =
      chrome_test_util::GetMainController()
          .interfaceProvider.mainInterface.viewController;
  return viewController.presentedViewController.keyCommands != nil;
}

@end
