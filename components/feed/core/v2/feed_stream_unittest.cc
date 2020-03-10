// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream_background.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

class TestFeedNetwork : public FeedNetwork {
 public:
  // FeedNetwork implementation.
  void SendQueryRequest(
      const feedwire::Request& request,
      base::OnceCallback<void(QueryRequestResult)> callback) override {}
  void SendActionRequest(
      const feedwire::ActionRequest& request,
      base::OnceCallback<void(ActionRequestResult)> callback) override {}
  void CancelRequests() override {}
};

class FakeRefreshTaskScheduler : public RefreshTaskScheduler {
 public:
  // RefreshTaskScheduler implementation.
  void EnsureScheduled(base::TimeDelta period) override {
    scheduled_period = period;
  }
  void Cancel() override { canceled = true; }
  void RefreshTaskComplete() override { refresh_task_complete = true; }

  base::Optional<base::TimeDelta> scheduled_period;
  bool canceled = false;
  bool refresh_task_complete = false;
};

class TestEventObserver : public FeedStream::EventObserver {
 public:
  // FeedStreamUnittest::StreamEventObserver.
  void OnMaybeTriggerRefresh(TriggerType trigger,
                             bool clear_all_before_refresh) override {
    refresh_trigger_type = trigger;
  }
  void OnClearAll(base::TimeDelta time_since_last_clear) override {
    this->time_since_last_clear = time_since_last_clear;
  }

  // Test access.

  base::Optional<base::TimeDelta> time_since_last_clear;
  base::Optional<TriggerType> refresh_trigger_type;
};

class FeedStreamTest : public testing::Test, public FeedStream::Delegate {
 public:
  void SetUp() override {
    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    stream_ = std::make_unique<FeedStream>(
        &refresh_scheduler_, &event_observer_, this, &profile_prefs_, &network_,
        &clock_, &tick_clock_, task_environment_.GetMainThreadTaskRunner());
  }

  // FeedStream::Delegate.
  bool IsEulaAccepted() override { return true; }
  bool IsOffline() override { return false; }

 protected:
  TestEventObserver event_observer_;
  TestingPrefServiceSimple profile_prefs_;
  TestFeedNetwork network_;
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
  FakeRefreshTaskScheduler refresh_scheduler_;
  std::unique_ptr<FeedStream> stream_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FeedStreamTest, IsArticlesListVisibleByDefault) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, SetArticlesListVisible) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(false);
  EXPECT_FALSE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(true);
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, RunInBackgroundAndReturn) {
  int result_received = 0;
  FeedStreamBackground* background_received = nullptr;
  base::RunLoop run_loop;
  auto quit = run_loop.QuitClosure();

  auto background_lambda =
      base::BindLambdaForTesting([&](FeedStreamBackground* bg) {
        background_received = bg;
        return 5;
      });
  auto result_lambda = base::BindLambdaForTesting([&](int result) {
    result_received = result;
    quit.Run();
  });

  stream_->RunInBackgroundAndReturn(FROM_HERE,
                                    base::BindOnce(background_lambda),
                                    base::BindOnce(result_lambda));

  run_loop.Run();

  EXPECT_TRUE(background_received);
  EXPECT_EQ(5, result_received);
}

TEST_F(FeedStreamTest, RefreshIsScheduledOnInitialize) {
  stream_->InitializeScheduling();
  EXPECT_TRUE(refresh_scheduler_.scheduled_period);
}

TEST_F(FeedStreamTest, ScheduledRefreshTriggersRefresh) {
  stream_->InitializeScheduling();
  stream_->ExecuteRefreshTask();

  EXPECT_EQ(TriggerType::kFixedTimer, event_observer_.refresh_trigger_type);
  // TODO(harringtond): Once we actually perform the refresh, make sure
  // RefreshTaskComplete() is called.
  // EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
}

TEST_F(FeedStreamTest, DoNotRefreshIfArticlesListIsHidden) {
  stream_->SetArticlesListVisible(false);
  stream_->InitializeScheduling();
  stream_->ExecuteRefreshTask();

  EXPECT_TRUE(refresh_scheduler_.canceled);
  EXPECT_FALSE(event_observer_.refresh_trigger_type);
}

}  // namespace
}  // namespace feed
