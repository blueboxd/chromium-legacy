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
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_history {

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
}

TEST_F(MediaHistoryStoreUnitTest, SavePlayback) {
  // Create a media player watch time and save it to the playbacks table.
  GURL url("http://google.com/test");
  content::MediaPlayerWatchTime watch_time(url, url.GetOrigin(),
                                           base::TimeDelta::FromSeconds(60),
                                           base::TimeDelta(), true, false);
  GetMediaHistoryStore()->SavePlayback(watch_time);
  int64_t now_in_seconds_before =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  // Save the watch time a second time.
  GetMediaHistoryStore()->SavePlayback(watch_time);

  // Wait until the playbacks have finished saving.
  content::RunAllTasksUntilIdle();

  int64_t now_in_seconds_after =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  // Verify that the playback table contains the expected number of items
  sql::Statement select_from_playback_statement(GetDB().GetUniqueStatement(
      "SELECT id, url, origin_id, watch_time_s, has_video, has_audio, "
      "last_updated_time_s FROM playback"));
  ASSERT_TRUE(select_from_playback_statement.is_valid());
  int playback_row_count = 0;
  while (select_from_playback_statement.Step()) {
    ++playback_row_count;
    EXPECT_EQ(playback_row_count, select_from_playback_statement.ColumnInt(0));
    EXPECT_EQ("http://google.com/test",
              select_from_playback_statement.ColumnString(1));
    EXPECT_EQ(1, select_from_playback_statement.ColumnInt(2));
    EXPECT_EQ(60, select_from_playback_statement.ColumnInt(3));
    EXPECT_EQ(1, select_from_playback_statement.ColumnInt(4));
    EXPECT_EQ(0, select_from_playback_statement.ColumnInt(5));
    EXPECT_LE(now_in_seconds_before,
              select_from_playback_statement.ColumnInt64(6));
    EXPECT_GE(now_in_seconds_after,
              select_from_playback_statement.ColumnInt64(6));
  }

  EXPECT_EQ(2, playback_row_count);

  // Verify that the origin table contains the expected number of items
  sql::Statement select_from_origin_statement(GetDB().GetUniqueStatement(
      "SELECT id, origin, last_updated_time_s FROM origin"));
  ASSERT_TRUE(select_from_origin_statement.is_valid());
  int origin_row_count = 0;
  while (select_from_origin_statement.Step()) {
    ++origin_row_count;
    EXPECT_EQ(1, select_from_origin_statement.ColumnInt(0));
    EXPECT_EQ("http://google.com/",
              select_from_origin_statement.ColumnString(1));
    EXPECT_LE(now_in_seconds_before,
              select_from_origin_statement.ColumnInt64(2));
    EXPECT_GE(now_in_seconds_after,
              select_from_origin_statement.ColumnInt64(2));
  }

  EXPECT_EQ(1, origin_row_count);
}

TEST_F(MediaHistoryStoreUnitTest, GetStats) {
  {
    // Check all the tables are empty.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(0,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);
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
      url_a, media_session::MediaMetadata(), base::nullopt);
  GetMediaHistoryStore()->SavePlaybackSession(
      url_b, media_session::MediaMetadata(), base::nullopt);

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
      url_a, media_session::MediaMetadata(), base::nullopt);

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

  const int64_t url_now_in_seconds_before =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  {
    // Record a video-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, false /* has_audio */);
    GetMediaHistoryStore()->SavePlayback(watch_time);
    content::RunAllTasksUntilIdle();
  }

  const int64_t url_now_in_seconds_after =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  {
    // Record a watchtime for audio/video for 60 seconds on a different origin.
    content::MediaPlayerWatchTime watch_time(
        url_alt, url_alt.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    GetMediaHistoryStore()->SavePlayback(watch_time);
    content::RunAllTasksUntilIdle();
  }

  const int64_t url_alt_now_in_seconds_after =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  {
    // Check the playbacks were recorded.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(5,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
  }

  // Verify that the origin table has the correct aggregate watchtime in
  // minutes.
  sql::Statement s(GetDB().GetUniqueStatement(
      "SELECT origin, aggregate_watchtime_audio_video_s, last_updated_time_s "
      "FROM origin"));
  ASSERT_TRUE(s.is_valid());

  EXPECT_TRUE(s.Step());
  EXPECT_EQ("http://google.com/", s.ColumnString(0));
  EXPECT_EQ(90, s.ColumnInt64(1));
  EXPECT_LE(url_now_in_seconds_before, s.ColumnInt64(2));
  EXPECT_GE(url_now_in_seconds_after, s.ColumnInt64(2));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ("http://example.org/", s.ColumnString(0));
  EXPECT_EQ(30, s.ColumnInt64(1));
  EXPECT_LE(url_now_in_seconds_after, s.ColumnInt64(2));
  EXPECT_GE(url_alt_now_in_seconds_after, s.ColumnInt64(2));

  EXPECT_FALSE(s.Step());
}

}  // namespace media_history
