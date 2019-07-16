// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread_v2.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {
constexpr auto kGpuWatchdogTimeout = base::TimeDelta::FromMilliseconds(1000);

// This task will run for duration_ms milliseconds.
void SimpleTask(base::TimeDelta duration) {
  base::PlatformThread::Sleep(duration);
}
}  // namespace

class GpuWatchdogTest : public testing::Test {
 public:
  GpuWatchdogTest() {}

  void LongTaskWithReportProgress(base::TimeDelta duration,
                                  base::TimeDelta report_delta);

  void LongTaskFromBackgroundToForeground(
      base::TimeDelta duration,
      base::TimeDelta time_to_switch_to_foreground);

  // Implements testing::Test
  void SetUp() override;

 protected:
  ~GpuWatchdogTest() override = default;
  base::MessageLoop main_loop;
  base::RunLoop run_loop;
  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;
};

void GpuWatchdogTest::SetUp() {
  ASSERT_TRUE(base::ThreadTaskRunnerHandle::IsSet());
  ASSERT_TRUE(base::MessageLoopCurrent::IsSet());

  // Set watchdog timeout to 1000 milliseconds
  watchdog_thread_ = gpu::GpuWatchdogThreadImplV2::Create(
      /*start_backgrounded*/ false,
      /*timeout*/ kGpuWatchdogTimeout,
      /*test_mode*/ true);
}

// This task will run for duration_ms milliseconds. It will also call watchdog
// ReportProgress() every report_delta_ms milliseconds.
void GpuWatchdogTest::LongTaskWithReportProgress(base::TimeDelta duration,
                                                 base::TimeDelta report_delta) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end;

  do {
    base::PlatformThread::Sleep(report_delta);
    watchdog_thread_->ReportProgress();
    end = base::TimeTicks::Now();
  } while (end - start <= duration);
}

void GpuWatchdogTest::LongTaskFromBackgroundToForeground(
    base::TimeDelta duration,
    base::TimeDelta time_to_switch_to_foreground) {
  // Chrome is running in the background first.
  watchdog_thread_->OnBackgrounded();
  base::PlatformThread::Sleep(time_to_switch_to_foreground);
  // Now switch Chrome to the foreground after the specified time
  watchdog_thread_->OnForegrounded();
  base::PlatformThread::Sleep(duration - time_to_switch_to_foreground);
}

// GPU Hang In Initialization
TEST_F(GpuWatchdogTest, GpuInitializationHang) {
  // Gpu init (4000 ms) takes longer than timeout (1000 ms).
  SimpleTask(kGpuWatchdogTimeout + base::TimeDelta::FromMilliseconds(3000));

  // Gpu hangs. OnInitComplete() is not called

  bool result = watchdog_thread_->IsGpuHangDetected();
  EXPECT_TRUE(result);
}

// Normal GPU Initialization and Running Task
TEST_F(GpuWatchdogTest, GpuInitializationAndRunningTasks) {
  // Assume GPU initialization takes 300 milliseconds.
  SimpleTask(base::TimeDelta::FromMilliseconds(300));
  watchdog_thread_->OnInitComplete();

  // Start running GPU tasks. Watchdog function WillProcessTask(),
  // DidProcessTask() and ReportProgress() are tested.
  main_loop.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, base::TimeDelta::FromMilliseconds(500)));
  main_loop.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, base::TimeDelta::FromMilliseconds(500)));

  // This long task takes 3000 milliseconds to finish, longer than timeout.
  // But it reports progress every 500 milliseconds
  main_loop.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuWatchdogTest::LongTaskWithReportProgress, base::Unretained(this),
          kGpuWatchdogTimeout + base::TimeDelta::FromMilliseconds(2000),
          base::TimeDelta::FromMilliseconds(500)));

  main_loop.task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Everything should be fine. No GPU hang detected.
  bool result = watchdog_thread_->IsGpuHangDetected();
  EXPECT_FALSE(result);
}

// GPU Hang when running a task
TEST_F(GpuWatchdogTest, GpuRunningATaskHang) {
  // Report gpu init complete
  watchdog_thread_->OnInitComplete();

  // Start running a GPU task. This long task takes 6000 milliseconds to finish.
  main_loop.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, kGpuWatchdogTimeout * 2 +
                                      base::TimeDelta::FromMilliseconds(4000)));

  main_loop.task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // This GPU task takes too long. A GPU hang should be detected.
  bool result = watchdog_thread_->IsGpuHangDetected();
  EXPECT_TRUE(result);
}

TEST_F(GpuWatchdogTest, ChromeInBackground) {
  // Chrome starts in the background.
  watchdog_thread_->OnBackgrounded();

  // Gpu init (2000 ms) takes longer than timeout (1000 ms).
  SimpleTask(kGpuWatchdogTimeout + base::TimeDelta::FromMilliseconds(1000));

  // Report GPU init complete.
  watchdog_thread_->OnInitComplete();

  // Run a task that takes longer (3000 milliseconds) than timeout.
  main_loop.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, kGpuWatchdogTimeout * 2 +
                                      base::TimeDelta::FromMilliseconds(1000)));
  main_loop.task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // The gpu might be slow when running in the background. This is ok.
  bool result = watchdog_thread_->IsGpuHangDetected();
  EXPECT_FALSE(result);
}

TEST_F(GpuWatchdogTest, GpuSwitchingToForegroundHang) {
  // Report GPU init complete.
  watchdog_thread_->OnInitComplete();

  // A task stays in the background for 200 milliseconds, and then
  // switches to the foreground and runs for 6000 milliseconds. This is longer
  // than the first-time foreground watchdog timeout (2000 ms).
  main_loop.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogTest::LongTaskFromBackgroundToForeground,
                     base::Unretained(this),
                     /*duration*/ kGpuWatchdogTimeout * 2 +
                         base::TimeDelta::FromMilliseconds(4200),
                     /*time_to_switch_to_foreground*/
                     base::TimeDelta::FromMilliseconds(200)));

  main_loop.task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // It takes too long to finish a task after switching to the foreground.
  // A GPU hang should be detected.
  bool result = watchdog_thread_->IsGpuHangDetected();
  EXPECT_TRUE(result);
}

}  // namespace gpu
