// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_history {

namespace {

// The error margin for double time comparison. It is 10 seconds because it
// might be equal but it might be close too.
const int kTimeErrorMargin = 10000;

}  // namespace

class MediaHistoryStoreUnitTest : public testing::Test {
 public:
  MediaHistoryStoreUnitTest() = default;
  void SetUp() override {
    // Set up the profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath());

    // Set up the media history store.
    scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner =
        base::CreateUpdateableSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::WithBaseSyncPrimitives()});
    media_history_store_ = std::make_unique<MediaHistoryStore>(
        profile_builder.Build().get(), task_runner);

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables before proceeding with the tests and
    // tearing down the temporary directory.
    content::RunAllTasksUntilIdle();

    // Set up the local DB connection used for assertions.
    base::FilePath db_file =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Media History"));
    ASSERT_TRUE(db_.Open(db_file));
  }

  void TearDown() override { content::RunAllTasksUntilIdle(); }

  mojom::MediaHistoryStatsPtr GetStatsSync() {
    base::RunLoop run_loop;
    mojom::MediaHistoryStatsPtr stats_out;

    GetMediaHistoryStore()->GetMediaHistoryStats(
        base::BindLambdaForTesting([&](mojom::MediaHistoryStatsPtr stats) {
          stats_out = std::move(stats);
          run_loop.Quit();
        }));

    run_loop.Run();
    return stats_out;
  }

  std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginRowsSync() {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryOriginRowPtr> out;

    GetMediaHistoryStore()->GetOriginRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryOriginRowPtr> rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  std::vector<mojom::MediaHistoryPlaybackRowPtr> GetPlaybackRowsSync() {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryPlaybackRowPtr> out;

    GetMediaHistoryStore()->GetMediaHistoryPlaybackRowsForDebug(
        base::BindLambdaForTesting(
            [&](std::vector<mojom::MediaHistoryPlaybackRowPtr> rows) {
              out = std::move(rows);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  MediaHistoryStore* GetMediaHistoryStore() {
    return media_history_store_.get();
  }

 private:
  base::ScopedTempDir temp_dir_;

 protected:
  sql::Database& GetDB() { return db_; }
  content::BrowserTaskEnvironment task_environment_;

 private:
  sql::Database db_;
  std::unique_ptr<MediaHistoryStore> media_history_store_;
};

TEST_F(MediaHistoryStoreUnitTest, CreateDatabaseTables) {
  ASSERT_TRUE(GetDB().DoesTableExist("origin"));
  ASSERT_TRUE(GetDB().DoesTableExist("playback"));
  ASSERT_TRUE(GetDB().DoesTableExist("playbackSession"));
  ASSERT_TRUE(GetDB().DoesTableExist("sessionImage"));
  ASSERT_TRUE(GetDB().DoesTableExist("mediaImage"));
}

TEST_F(MediaHistoryStoreUnitTest, SavePlayback) {
  const auto now_before =
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToJsTime();

  // Create a media player watch time and save it to the playbacks table.
  GURL url("http://google.com/test");
  content::MediaPlayerWatchTime watch_time(url, url.GetOrigin(),
                                           base::TimeDelta::FromSeconds(60),
                                           base::TimeDelta(), true, false);
  GetMediaHistoryStore()->SavePlayback(watch_time);
  const auto now_after_a = base::Time::Now().ToJsTime();

  // Save the watch time a second time.
  GetMediaHistoryStore()->SavePlayback(watch_time);

  // Wait until the playbacks have finished saving.
  content::RunAllTasksUntilIdle();

  const auto now_after_b = base::Time::Now().ToJsTime();

  // Verify that the playback table contains the expected number of items.
  std::vector<mojom::MediaHistoryPlaybackRowPtr> playbacks =
      GetPlaybackRowsSync();
  EXPECT_EQ(2u, playbacks.size());

  EXPECT_EQ("http://google.com/test", playbacks[0]->url.spec());
  EXPECT_FALSE(playbacks[0]->has_audio);
  EXPECT_TRUE(playbacks[0]->has_video);
  EXPECT_EQ(base::TimeDelta::FromSeconds(60), playbacks[0]->watchtime);
  EXPECT_LE(now_before, playbacks[0]->last_updated_time);
  EXPECT_GE(now_after_a, playbacks[0]->last_updated_time);

  EXPECT_EQ("http://google.com/test", playbacks[1]->url.spec());
  EXPECT_FALSE(playbacks[1]->has_audio);
  EXPECT_TRUE(playbacks[1]->has_video);
  EXPECT_EQ(base::TimeDelta::FromSeconds(60), playbacks[1]->watchtime);
  EXPECT_LE(now_before, playbacks[1]->last_updated_time);
  EXPECT_GE(now_after_b, playbacks[1]->last_updated_time);

  // Verify that the origin table contains the expected number of items.
  std::vector<mojom::MediaHistoryOriginRowPtr> origins = GetOriginRowsSync();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ("http://google.com", origins[0]->origin.Serialize());
  EXPECT_LE(now_before, origins[0]->last_updated_time);
  EXPECT_GE(now_after_b, origins[0]->last_updated_time);
}

TEST_F(MediaHistoryStoreUnitTest, GetStats) {
  {
    // Check all the tables are empty.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(0,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);
    EXPECT_EQ(
        0, stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }

  {
    // Create a media player watch time and save it to the playbacks table.
    GURL url("http://google.com/test");
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromMilliseconds(123),
        base::TimeDelta::FromMilliseconds(321), true, false);
    GetMediaHistoryStore()->SavePlayback(watch_time);
  }

  {
    // Check the tables have records in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(1, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(1,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);
    EXPECT_EQ(
        0, stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }
}

TEST_F(MediaHistoryStoreUnitTest, UrlShouldBeUniqueForSessions) {
  GURL url_a("https://www.google.com");
  GURL url_b("https://www.example.org");

  {
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);
  }

  // Save a couple of sessions on different URLs.
  GetMediaHistoryStore()->SavePlaybackSession(
      url_a, media_session::MediaMetadata(), base::nullopt,
      std::vector<media_session::MediaImage>());
  GetMediaHistoryStore()->SavePlaybackSession(
      url_b, media_session::MediaMetadata(), base::nullopt,
      std::vector<media_session::MediaImage>());

  // Wait until the sessions have finished saving.
  content::RunAllTasksUntilIdle();

  {
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    sql::Statement s(GetDB().GetUniqueStatement(
        "SELECT id FROM playbackSession WHERE url = ?"));
    s.BindString(0, url_a.spec());
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(1, s.ColumnInt(0));
  }

  // Save a session on the first URL.
  GetMediaHistoryStore()->SavePlaybackSession(
      url_a, media_session::MediaMetadata(), base::nullopt,
      std::vector<media_session::MediaImage>());

  // Wait until the sessions have finished saving.
  content::RunAllTasksUntilIdle();

  {
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // The row for |url_a| should have been replaced so we should have a new ID.
    sql::Statement s(GetDB().GetUniqueStatement(
        "SELECT id FROM playbackSession WHERE url = ?"));
    s.BindString(0, url_a.spec());
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(3, s.ColumnInt(0));
  }
}

TEST_F(MediaHistoryStoreUnitTest, SavePlayback_IncrementAggregateWatchtime) {
  GURL url("http://google.com/test");
  GURL url_alt("http://example.org/test");

  const auto url_now_before = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    GetMediaHistoryStore()->SavePlayback(watch_time);
    content::RunAllTasksUntilIdle();
  }

  {
    // Record a watchtime for audio/video for 60 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(60),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    GetMediaHistoryStore()->SavePlayback(watch_time);
    content::RunAllTasksUntilIdle();
  }

  {
    // Record an audio-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), false /* has_video */, true /* has_audio */);
    GetMediaHistoryStore()->SavePlayback(watch_time);
    content::RunAllTasksUntilIdle();
  }

  {
    // Record a video-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, false /* has_audio */);
    GetMediaHistoryStore()->SavePlayback(watch_time);
    content::RunAllTasksUntilIdle();
  }

  const auto url_now_after = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 60 seconds on a different origin.
    content::MediaPlayerWatchTime watch_time(
        url_alt, url_alt.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    GetMediaHistoryStore()->SavePlayback(watch_time);
    content::RunAllTasksUntilIdle();
  }

  const auto url_alt_after = base::Time::Now().ToJsTime();

  {
    // Check the playbacks were recorded.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(5,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
  }

  std::vector<mojom::MediaHistoryOriginRowPtr> origins = GetOriginRowsSync();
  EXPECT_EQ(2u, origins.size());

  EXPECT_EQ("http://google.com", origins[0]->origin.Serialize());
  EXPECT_EQ(base::TimeDelta::FromSeconds(90),
            origins[0]->cached_audio_video_watchtime);
  EXPECT_NEAR(url_now_before, origins[0]->last_updated_time, kTimeErrorMargin);
  EXPECT_GE(url_now_after, origins[0]->last_updated_time);
  EXPECT_EQ(origins[0]->cached_audio_video_watchtime,
            origins[0]->actual_audio_video_watchtime);

  EXPECT_EQ("http://example.org", origins[1]->origin.Serialize());
  EXPECT_EQ(base::TimeDelta::FromSeconds(30),
            origins[1]->cached_audio_video_watchtime);
  EXPECT_NEAR(url_now_before, origins[1]->last_updated_time, kTimeErrorMargin);
  EXPECT_GE(url_alt_after, origins[1]->last_updated_time);
  EXPECT_EQ(origins[1]->cached_audio_video_watchtime,
            origins[1]->actual_audio_video_watchtime);
}

}  // namespace media_history
