// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/metrics_app_interface.h"

#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/ukm/ukm_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/testing/nserror_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constant for timeout while waiting for asynchronous sync and UKM operations.
const NSTimeInterval kSyncUKMOperationsTimeout = 10.0;

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
  return base::test::ios::WaitUntilConditionOrTimeout(kSyncUKMOperationsTimeout,
                                                      condition);
}

+ (uint64_t)UKMClientID {
  return metrics::UkmEGTestHelper::client_id();
}

+ (BOOL)UKMHasDummySource:(int64_t)sourceId {
  return metrics::UkmEGTestHelper::HasDummySource(sourceId);
}

+ (void)UKMRecordDummySource:(int64_t)sourceID {
  return metrics::UkmEGTestHelper::RecordDummySource(sourceID);
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
