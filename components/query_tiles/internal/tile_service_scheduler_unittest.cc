// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/query_tiles/internal/tile_service_scheduler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/prefs/testing_pref_service.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_store.h"
#include "components/query_tiles/switches.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::Invoke;

namespace query_tiles {
namespace {

class MockBackgroundTaskScheduler
    : public background_task::BackgroundTaskScheduler {
 public:
  MockBackgroundTaskScheduler() = default;
  ~MockBackgroundTaskScheduler() override = default;
  MOCK_METHOD1(Schedule, bool(const background_task::TaskInfo& task_info));
  MOCK_METHOD1(Cancel, void(int));
};

class TileServiceSchedulerTest : public testing::Test {
 public:
  TileServiceSchedulerTest() = default;
  ~TileServiceSchedulerTest() override = default;

  void SetUp() override {
    base::Time fake_now;
    EXPECT_TRUE(base::Time::FromString("05/18/20 01:00:00 AM", &fake_now));
    clock_.SetNow(fake_now);
    query_tiles::RegisterPrefs(prefs()->registry());
    auto policy = std::make_unique<net::BackoffEntry::Policy>();
    policy->num_errors_to_ignore = 0;
    policy->initial_delay_ms = 1 * 1000;
    policy->multiply_factor = 2;
    policy->jitter_factor = 0;
    policy->maximum_backoff_ms = 4 * 1000;
    policy->always_use_initial_delay = false;
    policy->entry_lifetime_ms = -1;
    tile_service_scheduler_ =
        TileServiceScheduler::Create(&mocked_native_scheduler_, &prefs_,
                                     &clock_, &tick_clock_, std::move(policy));
    EXPECT_CALL(
        *native_scheduler(),
        Cancel(static_cast<int>(background_task::TaskIds::QUERY_TILE_JOB_ID)));
    tile_service_scheduler()->CancelTask();
  }

 protected:
  MockBackgroundTaskScheduler* native_scheduler() {
    return &mocked_native_scheduler_;
  }
  TileServiceScheduler* tile_service_scheduler() {
    return tile_service_scheduler_.get();
  }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  base::test::TaskEnvironment task_environment_;

  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
  TestingPrefServiceSimple prefs_;
  MockBackgroundTaskScheduler mocked_native_scheduler_;

  std::unique_ptr<TileServiceScheduler> tile_service_scheduler_;
};

MATCHER_P2(TaskInfoEq,
           min_window_start_time_ms,
           max_window_start_time_ms,
           "Verify window range in TaskInfo.") {
  EXPECT_TRUE(arg.one_off_info.has_value());
  EXPECT_GE(arg.one_off_info->window_start_time_ms, min_window_start_time_ms);
  EXPECT_LE(arg.one_off_info->window_start_time_ms, max_window_start_time_ms)
      << "Actual window start time in ms: "
      << arg.one_off_info->window_start_time_ms;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kQueryTilesInstantBackgroundTask)) {
    EXPECT_EQ(arg.one_off_info->window_end_time_ms -
                  arg.one_off_info->window_start_time_ms,
              10 * 1000)
        << "Actual window end time in ms: "
        << arg.one_off_info->window_end_time_ms;
  } else {
    EXPECT_EQ(arg.one_off_info->window_end_time_ms -
                  arg.one_off_info->window_start_time_ms,
              TileConfig::GetOneoffTaskWindowInMs())
        << "Actual window end time in ms: "
        << arg.one_off_info->window_end_time_ms;
  }

  return true;
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedSuccess) {
  auto expected_range_start = TileConfig::GetScheduleIntervalInMs();
  auto expected_range_end =
      expected_range_start + TileConfig::GetMaxRandomWindowInMs();
  EXPECT_CALL(*native_scheduler(),
              Schedule(TaskInfoEq(expected_range_start, expected_range_end)));
  tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kSuccess);
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedSuccessInstantFetchOn) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      query_tiles::switches::kQueryTilesInstantBackgroundTask, "true");

  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kSuccess);
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedSuspend) {
  EXPECT_CALL(*native_scheduler(), Schedule(TaskInfoEq(4000, 4000)));
  tile_service_scheduler()->OnFetchCompleted(
      TileInfoRequestStatus::kShouldSuspend);
}

// Verify the failure will add delay that using test backoff policy.
TEST_F(TileServiceSchedulerTest, OnFetchCompletedFailure) {
  for (int i = 0; i <= 2; i++) {
    EXPECT_CALL(*native_scheduler(),
                Schedule(TaskInfoEq(1000 * pow(2, i), 1000 * pow(2, i))));
    tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kFailure);
  }
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedOtherStatus) {
  std::vector<TileInfoRequestStatus> other_status = {
      TileInfoRequestStatus::kInit};
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  for (const auto& status : other_status) {
    tile_service_scheduler()->OnFetchCompleted(status);
  }
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedWithNoTiles) {
  auto expected_range_start = TileConfig::GetScheduleIntervalInMs();
  auto expected_range_end =
      expected_range_start + TileConfig::GetMaxRandomWindowInMs();
  EXPECT_CALL(*native_scheduler(),
              Schedule(TaskInfoEq(expected_range_start, expected_range_end)));
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedInstantFetchOn) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      query_tiles::switches::kQueryTilesInstantBackgroundTask, "true");
  EXPECT_CALL(*native_scheduler(), Schedule(TaskInfoEq(10 * 1000, 10 * 1000)));
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kSuccess);
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedWithFailure) {
  EXPECT_CALL(*native_scheduler(), Schedule(TaskInfoEq(4000, 4000)));
  tile_service_scheduler()->OnTileManagerInitialized(
      TileGroupStatus::kFailureDbOperation);
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedWithOtherStatus) {
  std::vector<TileGroupStatus> other_status = {TileGroupStatus::kUninitialized,
                                               TileGroupStatus ::kSuccess};
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  for (const auto status : other_status) {
    tile_service_scheduler()->OnTileManagerInitialized(status);
  }
}

}  // namespace
}  // namespace query_tiles
