// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_time_metrics.h"

#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

void WorkForOneCpuSec(base::WaitableEvent* event) {
  auto initial_ticks = base::ThreadTicks::Now();
  while (!event->IsSignaled()) {
    if (base::ThreadTicks::Now() >
        initial_ticks + base::TimeDelta::FromSeconds(1)) {
      event->Signal();
    }
  }
}

TEST(CpuTimeMetricsTest, RecordsMetrics) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histograms;
  base::Thread thread1("StackSamplingProfiler");

  thread1.StartAndWaitForTesting();
  ASSERT_TRUE(thread1.IsRunning());

  base::WaitableEvent event;

  thread1.task_runner()->PostTask(
      FROM_HERE, BindOnce(&WorkForOneCpuSec, base::Unretained(&event)));

  // Wait until the thread has consumed one second of CPU time.
  event.Wait();

  // Update current metrics.
  SampleCpuTimeMetricsForTesting();

  // The test process has no process-type command line flag, so is recognized as
  // the browser process. The thread created above is named like a sampling
  // profiler thread.
  static constexpr int kBrowserProcessBucket = 2;
  static constexpr int kSamplingProfilerThreadBucket = 24;

  // Expect that the CPU second spent by the thread above is represented in the
  // metrics.
  int browser_cpu_seconds = histograms.GetBucketCount(
      "Power.CpuTimeSecondsPerProcessType", kBrowserProcessBucket);
  EXPECT_GE(browser_cpu_seconds, 1);

  int thread_cpu_seconds =
      histograms.GetBucketCount("Power.CpuTimeSecondsPerThreadType.Browser",
                                kSamplingProfilerThreadBucket);
  EXPECT_GE(thread_cpu_seconds, 1);

  thread1.Stop();
}

}  // namespace
}  // namespace content