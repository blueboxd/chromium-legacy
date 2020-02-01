// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service.h"

#include <memory>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_history {

namespace {

base::FilePath g_temp_history_dir;

std::unique_ptr<KeyedService> BuildTestHistoryService(
    scoped_refptr<base::SequencedTaskRunner> backend_runner,
    content::BrowserContext* context) {
  std::unique_ptr<history::HistoryService> service(
      new history::HistoryService());
  service->set_backend_task_runner_for_testing(std::move(backend_runner));
  service->Init(history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return service;
}

}  // namespace

class MediaHistoryKeyedServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {history::HistoryService::kHistoryServiceUsesTaskScheduler}, {});

    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    g_temp_history_dir = temp_dir_.GetPath();

    mock_time_task_runner_ =
        base::MakeRefCounted<base::TestMockTimeTaskRunner>();

    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&BuildTestHistoryService, mock_time_task_runner_));

    service_ = std::make_unique<MediaHistoryKeyedService>(profile());

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables.
    content::RunAllTasksUntilIdle();
  }

  MediaHistoryKeyedService* service() const { return service_.get(); }

  void ConfigureHistoryService(
      scoped_refptr<base::SequencedTaskRunner> backend_runner) {
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildTestHistoryService,
                                       std::move(backend_runner)));
  }

  void TearDown() override {
    service_->Shutdown();

    // Tests that run a history service that uses the mock task runner for
    // backend processing will post tasks there during TearDown. Run them now to
    // avoid leaks.
    mock_time_task_runner_->RunUntilIdle();
    service_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  int GetUserDataTableRowCount() {
    int count = 0;
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();

    for (auto& entry : stats->table_row_counts) {
      // The meta table should not count as it does not contain any user data.
      if (entry.first == "meta")
        continue;

      count += entry.second;
    }

    return count;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

 private:
  mojom::MediaHistoryStatsPtr GetStatsSync() {
    base::RunLoop run_loop;
    mojom::MediaHistoryStatsPtr stats_out;

    service()->GetMediaHistoryStore()->GetMediaHistoryStats(base::BindOnce(
        [](mojom::MediaHistoryStatsPtr* stats_out,
           base::RepeatingClosure callback, mojom::MediaHistoryStatsPtr stats) {
          stats_out->Swap(&stats);
          std::move(callback).Run();
        },
        &stats_out, run_loop.QuitClosure()));

    run_loop.Run();
    return stats_out;
  }

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<MediaHistoryKeyedService> service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MediaHistoryKeyedServiceTest, CleanUpDatabaseWhenHistoryIsDeleted) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);
  GURL url("http://google.com/test");

  EXPECT_EQ(0, GetUserDataTableRowCount());

  // Record a playback in the database.
  {
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromMilliseconds(123),
        base::TimeDelta::FromMilliseconds(321), true, false);

    history->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
    service()->GetMediaHistoryStore()->SavePlayback(watch_time);

    // Wait until the playbacks have finished saving.
    content::RunAllTasksUntilIdle();
  }

  EXPECT_EQ(2, GetUserDataTableRowCount());

  {
    base::CancelableTaskTracker task_tracker;

    // Clear all history.
    history->ExpireHistoryBetween(std::set<GURL>(), base::Time(), base::Time(),
                                  /* user_initiated */ true, base::DoNothing(),
                                  &task_tracker);

    mock_time_task_runner_->RunUntilIdle();

    // Wait for the database to update.
    content::RunAllTasksUntilIdle();
  }

  EXPECT_EQ(0, GetUserDataTableRowCount());
}

}  // namespace media_history
