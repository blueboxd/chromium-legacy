// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/token.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

ServiceProcessInfo MakeFakeAudioServiceProcessInfo(base::ProcessId pid) {
  ServiceProcessInfo fake_audio_process_info;
  fake_audio_process_info.pid = pid;
  fake_audio_process_info.service_interface_name =
      audio::mojom::AudioService::Name_;
  return fake_audio_process_info;
}

}  // namespace

struct AudioServiceListenerTest : public testing::Test {
  AudioServiceListenerTest() {
    // This test environment is not set up to support out-of-process services.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAudioServiceOutOfProcess});
    test_clock.SetNowTicks(base::TimeTicks::Now());
  }

  base::test::ScopedFeatureList feature_list_;
  BrowserTaskEnvironment task_environment_;
  base::SimpleTestTickClock test_clock;
  base::HistogramTester histogram_tester;
};

TEST_F(AudioServiceListenerTest,
       ServiceCreatedStartedStopped_LogsStartupTime_LogsUptime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceCreated();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(42));
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount(
      "Media.AudioService.ObservedStartupTime",
      base::TimeDelta::FromMilliseconds(42), 1);
  test_clock.Advance(base::TimeDelta::FromDays(2));
  metrics.ServiceStopped();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedUptime",
                                         base::TimeDelta::FromDays(2), 1);

  test_clock.Advance(base::TimeDelta::FromHours(5));
  metrics.ServiceCreated();
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedDowntime2",
                                         base::TimeDelta::FromHours(5), 1);
}

TEST_F(AudioServiceListenerTest,
       CreateMetricsStartService_LogsInitialDowntime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  test_clock.Advance(base::TimeDelta::FromHours(12));
  metrics.ServiceCreated();
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount(
      "Media.AudioService.ObservedInitialDowntime",
      base::TimeDelta::FromHours(12), 1);
}

TEST_F(AudioServiceListenerTest, ServiceAlreadyRunningStopService_LogsUptime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceAlreadyRunning();
  test_clock.Advance(base::TimeDelta::FromMinutes(42));
  metrics.ServiceStopped();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedUptime",
                                         base::TimeDelta::FromMinutes(42), 1);
}

TEST_F(AudioServiceListenerTest,
       ServiceAlreadyRunningCreateService_LogsStartupTime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceAlreadyRunning();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(2));
  metrics.ServiceStopped();
  metrics.ServiceCreated();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(20));
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount(
      "Media.AudioService.ObservedStartupTime",
      base::TimeDelta::FromMilliseconds(20), 1);
}

// Check that if service was already created and ServiceStarted() is called,
// ObservedStartupTime and ObservedInitialDowntime are not logged and start time
// is reset.
TEST_F(AudioServiceListenerTest,
       ServiceAlreadyRunningStartService_ResetStartTime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceAlreadyRunning();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(20));
  metrics.ServiceStarted();
  histogram_tester.ExpectTotalCount("Media.AudioService.ObservedStartupTime",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Media.AudioService.ObservedInitialDowntime", 0);
  test_clock.Advance(base::TimeDelta::FromMilliseconds(200));
  metrics.ServiceStopped();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedUptime",
                                         base::TimeDelta::FromMilliseconds(200),
                                         1);
}

TEST_F(AudioServiceListenerTest, StartService_LogStartStatus) {
  base::HistogramTester histogram_tester;
  AudioServiceListener audio_service_listener;
  constexpr base::ProcessId pid(42);
  ServiceProcessInfo audio_process_info = MakeFakeAudioServiceProcessInfo(pid);
  audio_service_listener.Init({audio_process_info});
  histogram_tester.ExpectBucketCount(
      "Media.AudioService.ObservedStartStatus",
      static_cast<int>(
          AudioServiceListener::Metrics::ServiceStartStatus::kAlreadyStarted),
      1);
  audio_service_listener.OnServiceProcessTerminatedNormally(audio_process_info);

  audio_service_listener.OnServiceProcessLaunched(audio_process_info);
  histogram_tester.ExpectBucketCount(
      "Media.AudioService.ObservedStartStatus",
      static_cast<int>(
          AudioServiceListener::Metrics::ServiceStartStatus::kSuccess),
      1);
  audio_service_listener.OnServiceProcessTerminatedNormally(audio_process_info);
}

TEST_F(AudioServiceListenerTest, OnInitWithAudioService_ProcessIdNotNull) {
  AudioServiceListener audio_service_listener;
  constexpr base::ProcessId pid(42);
  ServiceProcessInfo audio_process_info = MakeFakeAudioServiceProcessInfo(pid);
  audio_service_listener.Init({audio_process_info});
  EXPECT_EQ(pid, audio_service_listener.GetProcessId());
}

}  // namespace content
