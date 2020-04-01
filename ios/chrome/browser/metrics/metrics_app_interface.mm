// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/metrics_app_interface.h"

#include <memory>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/sync/base/pref_names.h"
#include "components/ukm/ukm_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/testing/nserror_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"
#include "third_party/zlib/google/compression_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

bool g_metrics_enabled = false;

chrome_test_util::HistogramTester* g_histogram_tester = nullptr;
}  // namespace

namespace metrics {

// Helper class that provides access to UKM internals.
// This class is a friend of UKMService and UkmRecorderImpl.
class UkmEGTestHelper {
 public:
  UkmEGTestHelper() {}
  UkmEGTestHelper(const UkmEGTestHelper&) = delete;
  UkmEGTestHelper& operator=(UkmEGTestHelper&) = delete;

  static bool ukm_enabled() {
    auto* service = ukm_service();
    return service ? service->recording_enabled_ : false;
  }

  static bool IsReportUserNoisedUserBirthYearAndGenderEnabled() {
    return base::FeatureList::IsEnabled(
        ukm::UkmService::kReportUserNoisedUserBirthYearAndGender);
  }

  static uint64_t client_id() {
    auto* service = ukm_service();
    return service ? service->client_id_ : 0;
  }

  static bool HasDummySource(int64_t source_id_as_int64) {
    auto* service = ukm_service();
    ukm::SourceId source_id = source_id_as_int64;
    return service && base::Contains(service->sources(), source_id);
  }

  static void RecordDummySource(ukm::SourceId source_id_as_int64) {
    ukm::UkmService* service = ukm_service();
    ukm::SourceId source_id = source_id_as_int64;
    if (service)
      service->UpdateSourceURL(source_id, GURL("http://example.com"));
  }

  static void BuildAndStoreUkmLog() {
    ukm::UkmService* service = ukm_service();

    // Wait for initialization to complete before flushing.
    base::RunLoop run_loop;
    service->SetInitializationCompleteCallbackForTesting(
        run_loop.QuitClosure());
    run_loop.Run();

    service->Flush();
  }

  static bool HasUnsentLogs() {
    ukm::UkmService* service = ukm_service();
    return service->reporting_service_.ukm_log_store()->has_unsent_logs();
  }

  static std::unique_ptr<ukm::Report> GetUKMReport() {
    if (!HasUnsentLogs()) {
      return nullptr;
    }

    metrics::UnsentLogStore* log_store =
        ukm_service()->reporting_service_.ukm_log_store();
    if (log_store->has_staged_log()) {
      // For testing purposes, we are examining the content of a staged log
      // without ever sending the log, so discard any previously staged log.
      log_store->DiscardStagedLog();
    }
    log_store->StageNextLog();
    if (!log_store->has_staged_log()) {
      return nullptr;
    }

    std::string uncompressed_log_data;
    if (!compression::GzipUncompress(log_store->staged_log(),
                                     &uncompressed_log_data)) {
      return nullptr;
    }

    std::unique_ptr<ukm::Report> report = std::make_unique<ukm::Report>();
    if (!report->ParseFromString(uncompressed_log_data)) {
      return nullptr;
    }
    return report;
  }

 private:
  static ukm::UkmService* ukm_service() {
    return GetApplicationContext()
        ->GetMetricsServicesManager()
        ->GetUkmService();
  }
};

}  // namespace metrics

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
  ConditionBlock condition = ^{
    return metrics::UkmEGTestHelper::ukm_enabled() == enabled;
  };
  return base::test::ios::WaitUntilConditionOrTimeout(
      syncher::kSyncUKMOperationsTimeout, condition);
}

+ (BOOL)isReportUserNoisedUserBirthYearAndGenderEnabled {
  return metrics::UkmEGTestHelper::
      IsReportUserNoisedUserBirthYearAndGenderEnabled();
}

+ (uint64_t)UKMClientID {
  return metrics::UkmEGTestHelper::client_id();
}

+ (BOOL)UKMHasDummySource:(int64_t)sourceId {
  return metrics::UkmEGTestHelper::HasDummySource(sourceId);
}

+ (void)UKMRecordDummySource:(int64_t)sourceID {
  metrics::UkmEGTestHelper::RecordDummySource(sourceID);
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

+ (void)buildAndStoreUKMLog {
  metrics::UkmEGTestHelper::BuildAndStoreUkmLog();
}

+ (BOOL)hasUnsentLogs {
  return metrics::UkmEGTestHelper::HasUnsentLogs();
}

+ (BOOL)UKMReportHasBirthYear:(int)year gender:(int)gender {
  std::unique_ptr<ukm::Report> report =
      metrics::UkmEGTestHelper::GetUKMReport();

  int birthYearOffset =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLastUsedBrowserState()
          ->GetPrefs()
          ->GetInteger(syncer::prefs::kSyncDemographicsBirthYearOffset);
  int noisedBirthYear = year + birthYearOffset;

  return report && gender == report->user_demographics().gender() &&
         noisedBirthYear == report->user_demographics().birth_year();
}

+ (BOOL)UKMReportHasUserDemographics {
  std::unique_ptr<ukm::Report> report =
      metrics::UkmEGTestHelper::GetUKMReport();
  return report && report->has_user_demographics();
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
