// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_METRICS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_METRICS_METRICS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// MetricsAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface MetricsAppInterface : NSObject

// Enable/Disable the metrics in the app for test.
// |overrideMetricsAndCrashReportingForTesting| must be called before setting
// the value
// |stopOverridingMetricsAndCrashReportingForTesting| must be called at the end
// of the test for cleanup.
// |setMetricsAndCrashReportingForTesting:| can be called to set to
// enable/disable metrics. It returns whether metrics were previously enabled.
+ (void)overrideMetricsAndCrashReportingForTesting;
+ (void)stopOverridingMetricsAndCrashReportingForTesting;
+ (BOOL)setMetricsAndCrashReportingForTesting:(BOOL)enabled;

// Returns whether the UKM recording is |enabled|.
+ (BOOL)checkUKMRecordingEnabled:(BOOL)enabled;

// Returns the current UKM client ID.
+ (uint64_t)UKMClientID;

// Checks whether a sourceID is registered for UKM.
+ (BOOL)UKMHasDummySource:(int64_t)sourceId;

// Adds a new sourceID for UKM.
+ (void)UKMRecordDummySource:(int64_t)sourceId;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_METRICS_APP_INTERFACE_H_
