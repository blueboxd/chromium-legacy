// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/jank_monitor.h"

#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {

class TestObserver : public JankMonitor::Observer {
 public:
  void OnJankStarted() override { jank_started_called_++; }
  void OnJankStopped() override { jank_stopped_called_++; }

  int jank_started_called() { return jank_started_called_; }
  int jank_stopped_called() { return jank_stopped_called_; }

 private:
  int jank_started_called_ = 0;
  int jank_stopped_called_ = 0;
};

class TestMetricSource : public MetricSource {
 public:
  TestMetricSource(Delegate* delegate)
      : MetricSource(delegate) {}
  ~TestMetricSource() override {}

  std::unique_ptr<NativeEventObserver> CreateNativeEventObserver() override {
    return nullptr;
  }
};

class TestJankMonitor : public JankMonitor {
 public:
  TestJankMonitor() {}

  bool destroy_on_monitor_thread_called() {
    return destroy_on_monitor_thread_called_;
  }

  void SetOnDestroyedCallback(base::OnceClosure on_destroyed) {
    on_destroyed_ = std::move(on_destroyed);
  }

  void RunMonitorThreadUntilIdle() { mock_task_runner_->RunUntilIdle(); }
  void FastForwardMonitorThreadBy(base::TimeDelta delta) {
    mock_task_runner_->FastForwardBy(delta);
  }

  using JankMonitor::timer_running;

 protected:
  friend class base::RefCounted<TestJankMonitor>;

  ~TestJankMonitor() override {
    if (on_destroyed_)
      std::move(on_destroyed_).Run();
  }

  std::unique_ptr<MetricSource> CreateMetricSource() override {
    return std::make_unique<TestMetricSource>(this);
  }

  void DestroyOnMonitorThread() override {
    destroy_on_monitor_thread_called_ = true;
    JankMonitor::DestroyOnMonitorThread();
  }

  scoped_refptr<base::SequencedTaskRunner> CreateMonitorTaskRunner() override {
    mock_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    return mock_task_runner_;
  }

 private:
  bool destroy_on_monitor_thread_called_ = false;
  base::OnceClosure on_destroyed_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
};

class JankMonitorTest : public testing::Test {
 public:
  JankMonitorTest()
      : test_browser_thread_bundle_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW) {}
  ~JankMonitorTest() override {}

  void SetUp() override {
    monitor_ = base::MakeRefCounted<TestJankMonitor>();
    monitor_->SetUp();
    monitor_->AddObserver(&test_observer_);
    RunAllThreadsUntilIdle();
  }

  void TearDown() override {
    if (!monitor_)  // Already teared down.
      return;
    monitor_->Destroy();
    RunAllThreadsUntilIdle();
    monitor_ = nullptr;
  }

 protected:
  void RunAllThreadsUntilIdle() {
    test_browser_thread_bundle_.RunUntilIdle();
    monitor_->RunMonitorThreadUntilIdle();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void FastForwardAllThreadsByMs(int time_delta_ms) {
    auto delta = base::TimeDelta::FromMilliseconds(time_delta_ms);
    test_browser_thread_bundle_.FastForwardBy(delta);
    monitor_->FastForwardMonitorThreadBy(delta);
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  scoped_refptr<TestJankMonitor> monitor_;
  TestObserver test_observer_;
  int expected_jank_started_ = 0;
  int expected_jank_stopped_ = 0;
};

// Test life cycle management of the jank monitor: DestroyOnMonitorThread()
// and dtor.
TEST_F(JankMonitorTest, LifeCycle) {
  bool monitor_destroyed = false;
  auto closure =
      base::BindLambdaForTesting([&]() { monitor_destroyed = true; });
  monitor_->SetOnDestroyedCallback(std::move(closure));

  EXPECT_FALSE(monitor_->destroy_on_monitor_thread_called());

  // Test that the monitor thread is destroyed.
  monitor_->Destroy();
  RunAllThreadsUntilIdle();
  EXPECT_TRUE(monitor_->destroy_on_monitor_thread_called());

  // Release the last reference to TestJankMonitor. Check that it doesn't leak.
  monitor_ = nullptr;
  EXPECT_TRUE(monitor_destroyed);
}

#define VALIDATE_TEST_OBSERVER_CALLS()                                       \
  do {                                                                       \
    EXPECT_EQ(test_observer_.jank_started_called(), expected_jank_started_); \
    EXPECT_EQ(test_observer_.jank_stopped_called(), expected_jank_stopped_); \
  } while (0)

// Test monitor with UI thread janks.
TEST_F(JankMonitorTest, JankUIThread) {
  auto janky_task = [&]() {
    VALIDATE_TEST_OBSERVER_CALLS();
    // This is a janky task that runs for 1.5 seconds.
    expected_jank_started_++;
    FastForwardAllThreadsByMs(1500);
    RunAllThreadsUntilIdle();

    // Monitor should observe that the jank has started.
    VALIDATE_TEST_OBSERVER_CALLS();
    expected_jank_stopped_++;
  };

  // Post a janky task to the UI thread. Number of callback calls should be
  // incremented by 1.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindLambdaForTesting(janky_task));
  RunAllThreadsUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();

  // Post a non janky task. Number of callback calls should remain the same.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::DoNothing());
  RunAllThreadsUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();

  // Post a janky task again. Monitor thread timer should fire again. Number of
  // callback calls should be incremented by 1 again.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindLambdaForTesting(janky_task));
  RunAllThreadsUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test monitor with an IO thread jank.
TEST_F(JankMonitorTest, JankIOThread) {
  auto janky_task = [&]() {
    VALIDATE_TEST_OBSERVER_CALLS();

    // This is a janky task that runs for 1.5 seconds.
    expected_jank_started_++;
    FastForwardAllThreadsByMs(1500);
    RunAllThreadsUntilIdle();

    // Monitor should observe that the jank has started.
    VALIDATE_TEST_OBSERVER_CALLS();

    expected_jank_stopped_++;
  };

  // Post a janky task to the IO thread. This should increment the number of
  // callback calls by 1.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           base::BindLambdaForTesting(janky_task));
  RunAllThreadsUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test monitor with a reentrant UI thread task. The reentrant task shouldn't
// be reported by the monitor.
TEST_F(JankMonitorTest, JankUIThreadReentrant) {
  auto janky_task = [&]() {
    VALIDATE_TEST_OBSERVER_CALLS();

    // This is a janky task that runs for 1.5 seconds.
    expected_jank_started_++;
    FastForwardAllThreadsByMs(1500);
    RunAllThreadsUntilIdle();

    // Monitor should observe that the jank has started.
    VALIDATE_TEST_OBSERVER_CALLS();

    auto nested_janky_task = [&]() {
      // This also janks the current thread.
      FastForwardAllThreadsByMs(1500);

      // The callback shouldn't be called.
      VALIDATE_TEST_OBSERVER_CALLS();
    };
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindLambdaForTesting(nested_janky_task));
    // Spin a nested run loop to run |nested_janky_task|.
    base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
    expected_jank_stopped_++;
  };

  // Post a janky task to the UI thread. Number of callback calls should be
  // incremented by 1.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindLambdaForTesting(janky_task));
  RunAllThreadsUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test monitor with overlapping janks on both threads. Only the jank started
// first should be reported.
TEST_F(JankMonitorTest, JankUIAndIOThread) {
  auto janky_task_ui = [&]() {
    expected_jank_started_++;

    // This should trigger the monitor. TestJankMonitor::OnJankStarted() should
    // be called once.
    FastForwardAllThreadsByMs(1500);
    RunAllThreadsUntilIdle();
    VALIDATE_TEST_OBSERVER_CALLS();

    // The IO thread is also janky.
    auto janky_task_io = [&]() {
      // This is a janky task that runs for 1.5 seconds, but shouldn't trigger
      // the monitor.
      FastForwardAllThreadsByMs(1500);
      RunAllThreadsUntilIdle();
      VALIDATE_TEST_OBSERVER_CALLS();

      // Monitor should observe that the jank has started.
    };
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                             base::BindLambdaForTesting(janky_task_io));
    RunAllThreadsUntilIdle();
    // TestJankMonitor::OnJankStopped() shouldn't be called.
    VALIDATE_TEST_OBSERVER_CALLS();

    FastForwardAllThreadsByMs(500);
    expected_jank_stopped_++;
  };
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindLambdaForTesting(janky_task_ui));
  RunAllThreadsUntilIdle();
  // Expect that TestJankMonitor::OnJankStopped() was called.
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test stopping monitor timer when there is no activity and starting monitor
// timer on new activity.
TEST_F(JankMonitorTest, StartStopTimer) {
  // Activity on the UI thread - timer should be running.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(base::DoNothing::Once()));
  RunAllThreadsUntilIdle();
  EXPECT_TRUE(monitor_->timer_running());

  // 11 seconds passed with no activity - timer should be stopped.
  FastForwardAllThreadsByMs(11 * 1000);
  RunAllThreadsUntilIdle();
  EXPECT_FALSE(monitor_->timer_running());

  // Activity on IO thread - timer should be restarted.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           base::BindOnce(base::DoNothing::Once()));
  RunAllThreadsUntilIdle();
  EXPECT_TRUE(monitor_->timer_running());

  // 11 seconds passed with no activity - timer should be stopped.
  FastForwardAllThreadsByMs(11 * 1000);
  RunAllThreadsUntilIdle();
  EXPECT_FALSE(monitor_->timer_running());
}

#undef VALIDATE_TEST_OBSERVER_CALLS

}  // namespace responsiveness.
}  // namespace content.
