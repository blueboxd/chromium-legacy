// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/metrics_app_interface.h"

#include <string>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/sync/base/pref_names.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/ukm_test_helper.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/testing/nserror_util.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/zlib/google/compression_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

bool g_metrics_enabled = false;

chrome_test_util::HistogramTester* g_histogram_tester = nullptr;

ukm::UkmService* GetUkmService() {
  return GetApplicationContext()->GetMetricsServicesManager()->GetUkmService();
}

// TODO(crbug.com/1066297): Refactor to remove duplicate code.
// Returns an UMA log if the metrics service has a staged log.
std::unique_ptr<metrics::ChromeUserMetricsExtension> GetLastUmaLog() {
  metrics::MetricsLogStore* const log_store =
      GetApplicationContext()->GetMetricsService()->LogStoreForTest();

  // Decompress the staged log.
  std::string uncompressed_log;
  if (!compression::GzipUncompress(log_store->staged_log(),
                                   &uncompressed_log)) {
    return nullptr;
  }

  // Deserialize and return the log.
  std::unique_ptr<metrics::ChromeUserMetricsExtension> uma_log =
      std::make_unique<metrics::ChromeUserMetricsExtension>();
  if (!uma_log->ParseFromString(uncompressed_log)) {
    return nullptr;
  }
  return uma_log;
}

}  // namespace

@implementation MetricsAppInterface : NSObject

+ (void)overrideMetricsAndCrashReportingForTesting {
  IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &g_metrics_enabled);
}

+ (void)stopOverridingMetricsAndCrashReportingForTesting {
  IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      nullptr);
}

+ (BOOL)setMetricsAndCrashReportingForTesting:(BOOL)enabled {
  BOOL previousValue = g_metrics_enabled;
  g_metrics_enabled = enabled;
  GetApplicationContext()->GetMetricsServicesManager()->UpdateUploadPermissions(
      true);
  return previousValue;
}

+ (BOOL)checkUKMRecordingEnabled:(BOOL)enabled {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());

  ConditionBlock condition = ^{
    return ukm_test_helper.IsRecordingEnabled() == enabled;
  };
  return base::test::ios::WaitUntilConditionOrTimeout(
      syncher::kSyncUKMOperationsTimeout, condition);
}

+ (BOOL)isReportUserNoisedUserBirthYearAndGenderEnabled {
  return ukm::UkmTestHelper::IsReportUserNoisedUserBirthYearAndGenderEnabled();
}

+ (uint64_t)UKMClientID {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  return ukm_test_helper.GetClientId();
}

+ (BOOL)UKMHasDummySource:(int64_t)sourceID {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  return ukm_test_helper.HasSource(sourceID);
}

+ (void)UKMRecordDummySource:(int64_t)sourceID {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ukm_test_helper.RecordSourceForTesting(sourceID);
}

+ (void)setNetworkTimeForTesting {
  // The resolution was arbitrarily chosen.
  base::TimeDelta resolution = base::TimeDelta::FromMilliseconds(17);

  // Simulate the latency in the network to get the network time from the remote
  // server.
  base::TimeDelta latency = base::TimeDelta::FromMilliseconds(50);

  base::DefaultClock clock;
  base::DefaultTickClock tickClock;
  GetApplicationContext()->GetNetworkTimeTracker()->UpdateNetworkTime(
      clock.Now() - latency / 2, resolution, latency, tickClock.NowTicks());
}

+ (int)noisedBirthYear:(int)rawBirthYear {
  int birthYearOffset =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLastUsedBrowserState()
          ->GetPrefs()
          ->GetInteger(syncer::prefs::kSyncDemographicsBirthYearOffset);
  return rawBirthYear + birthYearOffset;
}

+ (void)buildAndStoreUKMLog {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ukm_test_helper.BuildAndStoreLog();
}

+ (BOOL)hasUnsentUKMLogs {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  return ukm_test_helper.HasUnsentLogs();
}

+ (BOOL)UKMReportHasBirthYear:(int)year gender:(int)gender {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  int noisedBirthYear = [self noisedBirthYear:year];

  return report && gender == report->user_demographics().gender() &&
         noisedBirthYear == report->user_demographics().birth_year();
}

+ (BOOL)UKMReportHasUserDemographics {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  // TODO(crbug.com/1066910): Maybe ukm::Report*?
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  return report && report->has_user_demographics();
}

+ (void)buildAndStoreUMALog {
  GetApplicationContext()->GetMetricsService()->StageCurrentLogForTest();
}

+ (BOOL)hasUnsentUMALogs {
  return GetApplicationContext()
      ->GetMetricsService()
      ->LogStoreForTest()
      ->has_unsent_logs();
}

+ (BOOL)UMALogHasBirthYear:(int)year gender:(int)gender {
  if (![self UMALogHasUserDemographics]) {
    return NO;
  }
  std::unique_ptr<metrics::ChromeUserMetricsExtension> log = GetLastUmaLog();
  int noisedBirthYear = [self noisedBirthYear:year];

  return noisedBirthYear == log->user_demographics().birth_year() &&
         gender == log->user_demographics().gender();
}

+ (BOOL)UMALogHasUserDemographics {
  if (![self hasUnsentUMALogs]) {
    return NO;
  }
  std::unique_ptr<metrics::ChromeUserMetricsExtension> log = GetLastUmaLog();
  return log && log->has_user_demographics();
}

+ (NSError*)setupHistogramTester {
  if (g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"Cannot setup two histogram testers.");
  }
  g_histogram_tester = new chrome_test_util::HistogramTester();
  return nil;
}

+ (NSError*)releaseHistogramTester {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"Cannot release histogram tester.");
  }
  delete g_histogram_tester;
  g_histogram_tester = nullptr;
  return nil;
}

+ (NSError*)expectTotalCount:(int)count forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  __block NSString* error = nil;

  g_histogram_tester->ExpectTotalCount(base::SysNSStringToUTF8(histogram),
                                       count, ^(NSString* e) {
                                         error = e;
                                       });
  if (error) {
    return testing::NSErrorWithLocalizedDescription(error);
  }
  return nil;
}

+ (NSError*)expectCount:(int)count
              forBucket:(int)bucket
           forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  __block NSString* error = nil;

  g_histogram_tester->ExpectBucketCount(base::SysNSStringToUTF8(histogram),
                                        bucket, count, ^(NSString* e) {
                                          error = e;
                                        });
  if (error) {
    return testing::NSErrorWithLocalizedDescription(error);
  }
  return nil;
}

+ (NSError*)expectUniqueSampleWithCount:(int)count
                              forBucket:(int)bucket
                           forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  __block NSString* error = nil;

  g_histogram_tester->ExpectUniqueSample(base::SysNSStringToUTF8(histogram),
                                         bucket, count, ^(NSString* e) {
                                           error = e;
                                         });
  if (error) {
    return testing::NSErrorWithLocalizedDescription(error);
  }
  return nil;
}

+ (NSError*)expectSum:(NSInteger)sum forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  std::unique_ptr<base::HistogramSamples> samples =
      g_histogram_tester->GetHistogramSamplesSinceCreation(
          base::SysNSStringToUTF8(histogram));
  if (samples->sum() != sum) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:
            @"Sum of histogram %@ mismatch. Expected %ld, Observed: %ld",
            histogram, static_cast<long>(sum),
            static_cast<long>(samples->sum())]);
  }
  return nil;
}

@end
