// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_METRICS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_METRICS_METRICS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"

namespace syncher {

// Constant for timeout while waiting for asynchronous sync and UKM operations.
const NSTimeInterval kSyncUKMOperationsTimeout = 10.0;

}  // namespace syncher

// MetricsAppInterface contains the app-side implementation for helpers. These
// helpers are compiled into the app binary and can be called from either app or
// test code.
@interface MetricsAppInterface : NSObject

// Enable/Disable the metrics in the app for test.
// |overrideMetricsAndCrashReportingForTesting| must be called before setting
// the value.
// |stopOverridingMetricsAndCrashReportingForTesting| must be called at the end
// of the test for cleanup.
// |setMetricsAndCrashReportingForTesting:| can be called to enable/disable
// metrics. It returns whether metrics were previously enabled.
+ (void)overrideMetricsAndCrashReportingForTesting;
+ (void)stopOverridingMetricsAndCrashReportingForTesting;
+ (BOOL)setMetricsAndCrashReportingForTesting:(BOOL)enabled;

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// Returns whether UKM recording is |enabled|.
+ (BOOL)checkUKMRecordingEnabled:(BOOL)enabled;

// Returns YES if the ReportUserNoisedUserBirthYearAndGender feature is enabled.
+ (BOOL)isReportUserNoisedUserBirthYearAndGenderEnabled WARN_UNUSED_RESULT;

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// Returns the current UKM client ID.
+ (uint64_t)UKMClientID;

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// Checks whether a sourceID is registered for UKM.
+ (BOOL)UKMHasDummySource:(int64_t)sourceId;

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// Adds a new sourceID for UKM.
+ (void)UKMRecordDummySource:(int64_t)sourceId;

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// Sets the network time to approximately now.
+ (void)setNetworkTimeForTesting;

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// If new data are available, creates a UKM Report and stores it in the
// UKM service's UnsentLogStore.
+ (void)buildAndStoreUKMLog;

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// Returns YES if the UKM service has logs to send.
+ (BOOL)hasUnsentLogs;

// Returns YES if the UKM service's report has the expected year and gender.
// Gender corresponds to the options in UserDemographicsProto::Gender.
+ (BOOL)UKMReportHasBirthYear:(int)year gender:(int)gender;

// Returns YES if the UKM service's report has user demographics.
+ (BOOL)UKMReportHasUserDemographics;

// Creates a chrome_test_util::HistogramTester that will record every histogram
// sent during test.
+ (NSError*)setupHistogramTester WARN_UNUSED_RESULT;

// Releases the chrome_test_util::HistogramTester.
+ (NSError*)releaseHistogramTester WARN_UNUSED_RESULT;

// We don't know the values of the samples, but we know how many there are.
// This measures the diff from the snapshot taken when this object was
// constructed.
+ (NSError*)expectTotalCount:(int)count
                forHistogram:(NSString*)histogram WARN_UNUSED_RESULT;

// We know the exact number of samples in a bucket, but other buckets may
// have samples as well. Measures the diff from the snapshot taken when this
// object was constructed.
+ (NSError*)expectCount:(int)count
              forBucket:(int)bucket
           forHistogram:(NSString*)histogram WARN_UNUSED_RESULT;

// We know the exact number of samples in a bucket, and that no other bucket
// should have samples. Measures the diff from the snapshot taken when this
// object was constructed.
+ (NSError*)expectUniqueSampleWithCount:(int)count
                              forBucket:(int)bucket
                           forHistogram:(NSString*)histogram WARN_UNUSED_RESULT;

// Checks the sum of all samples recorder for |histogram|.
+ (NSError*)expectSum:(NSInteger)sum
         forHistogram:(NSString*)histogram WARN_UNUSED_RESULT;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_METRICS_APP_INTERFACE_H_
