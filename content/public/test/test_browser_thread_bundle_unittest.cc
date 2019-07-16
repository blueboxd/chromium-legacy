// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedTaskEnvironment;

namespace content {

namespace {

// TestBrowserThreadBundleTest.RunUntilIdle will run kNumTasks tasks that will
// hop back-and-forth between ThreadPool and UI thread kNumHops times.
// Note: These values are arbitrary.
constexpr int kNumHops = 13;
constexpr int kNumTasks = 8;

void PostTaskToUIThread(int iteration, base::subtle::Atomic32* tasks_run);

void PostToThreadPool(int iteration, base::subtle::Atomic32* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    base::subtle::NoBarrier_AtomicIncrement(tasks_run, 1);

  if (iteration == kNumHops)
    return;

  base::PostTask(FROM_HERE,
                 base::BindOnce(&PostTaskToUIThread, iteration + 1, tasks_run));
}

void PostTaskToUIThread(int iteration, base::subtle::Atomic32* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    base::subtle::NoBarrier_AtomicIncrement(tasks_run, 1);

  if (iteration == kNumHops)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&PostToThreadPool, iteration + 1, tasks_run));
}

}  // namespace

TEST(TestBrowserThreadBundleTest, RunUntilIdle) {
  TestBrowserThreadBundle test_browser_thread_bundle;

  base::subtle::Atomic32 tasks_run = 0;

  // Post half the tasks on ThreadPool and the other half on the UI thread
  // so they cross and the last hops aren't all on the same task runner.
  for (int i = 0; i < kNumTasks; ++i) {
    if (i % 2) {
      PostToThreadPool(0, &tasks_run);
    } else {
      PostTaskToUIThread(0, &tasks_run);
    }
  }

  test_browser_thread_bundle.RunUntilIdle();

  EXPECT_EQ(kNumTasks * kNumHops, base::subtle::NoBarrier_Load(&tasks_run));
}

namespace {

void PostRecurringTaskToIOThread(int iteration, int* tasks_run) {
  // All iterations but the first come from a task that was posted.
  if (iteration > 0)
    (*tasks_run)++;

  if (iteration == kNumHops)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PostRecurringTaskToIOThread, iteration + 1, tasks_run));
}

}  // namespace

TEST(TestBrowserThreadBundleTest, RunIOThreadUntilIdle) {
  TestBrowserThreadBundle test_browser_thread_bundle(
      TestBrowserThreadBundle::Options::REAL_IO_THREAD);

  int tasks_run = 0;

  for (int i = 0; i < kNumTasks; ++i) {
    PostRecurringTaskToIOThread(0, &tasks_run);
  }

  test_browser_thread_bundle.RunIOThreadUntilIdle();

  EXPECT_EQ(kNumTasks * kNumHops, tasks_run);
}

TEST(TestBrowserThreadBundleTest, MessageLoopTypeMismatch) {
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestBrowserThreadBundle test_browser_thread_bundle(
            TestBrowserThreadBundle::IO_MAINLOOP);
      },
      "");
}

TEST(TestBrowserThreadBundleTest, MultipleTestBrowserThreadBundle) {
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestBrowserThreadBundle test_browser_thread_bundle;
        TestBrowserThreadBundle other_test_browser_thread_bundle;
      },
      "");
}

TEST(TestBrowserThreadBundleTest, TraitsConstructor) {
  TestBrowserThreadBundle test_browser_thread_bundle(
      TestBrowserThreadBundle::Options::REAL_IO_THREAD,
      base::test::ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);
  // Should set up a UI main thread.
  EXPECT_TRUE(base::MessageLoopCurrentForUI::IsSet());
  EXPECT_FALSE(base::MessageLoopCurrentForIO::IsSet());

  // Should create a real IO thread. If it was on the same thread the following
  // will timeout.
  base::WaitableEvent signaled_on_real_io_thread;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&base::WaitableEvent::Signal,
                     Unretained(&signaled_on_real_io_thread)));
  signaled_on_real_io_thread.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_TRUE(signaled_on_real_io_thread.IsSignaled());

  // Tasks posted via PostTask don't run in ThreadPoolExecutionMode::QUEUED until
  // RunUntilIdle is called.
  base::AtomicFlag task_ran;
  PostTask(FROM_HERE,
           BindOnce([](base::AtomicFlag* task_ran) { task_ran->Set(); },
                    Unretained(&task_ran)));

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(task_ran.IsSet());

  test_browser_thread_bundle.RunUntilIdle();
  EXPECT_TRUE(task_ran.IsSet());
}

TEST(TestBrowserThreadBundleTest, TraitsConstructorOverrideMainThreadType) {
  TestBrowserThreadBundle test_browser_thread_bundle(
      base::test::ScopedTaskEnvironment::MainThreadType::UI,
      base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME);

  // Should set up a UI main thread.
  EXPECT_TRUE(base::MessageLoopCurrentForUI::IsSet());
  EXPECT_FALSE(base::MessageLoopCurrentForIO::IsSet());

  // There should be a mock clock.
  EXPECT_THAT(test_browser_thread_bundle.GetMockClock(), testing::NotNull());
}

}  // namespace content
