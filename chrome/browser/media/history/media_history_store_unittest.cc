// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_feed_items_table.h"
#include "chrome/browser/media/history/media_history_feeds_table.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/history/core/test/test_history_database.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
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

// The expected display name for the fetched media feed.
const char kExpectedDisplayName[] = "Test Feed";

// The expected counts and content types for the test feed.
const int kExpectedFetchItemCount = 3;
const int kExpectedFetchPlayNextCount = 2;
const int kExpectedFetchContentTypes =
    static_cast<int>(media_feeds::mojom::MediaFeedItemType::kMovie) |
    static_cast<int>(media_feeds::mojom::MediaFeedItemType::kTVSeries);

// The expected counts and content types for the alternate test feed.
const int kExpectedAltFetchItemCount = 1;
const int kExpectedAltFetchPlayNextCount = 1;
const int kExpectedAltFetchContentTypes =
    static_cast<int>(media_feeds::mojom::MediaFeedItemType::kVideo);

base::FilePath g_temp_history_dir;

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  std::unique_ptr<history::HistoryService> service(
      new history::HistoryService());
  service->Init(history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return service;
}

enum class TestState {
  kNormal,

  // Runs the test in incognito mode.
  kIncognito,

  // Runs the test with the "SavingBrowserHistoryDisabled" policy enabled.
  kSavingBrowserHistoryDisabled,
};

}  // namespace

// Runs the test with a param to signify the profile being incognito if true.
class MediaHistoryStoreUnitTest
    : public testing::Test,
      public testing::WithParamInterface<TestState> {
 public:
  MediaHistoryStoreUnitTest() = default;
  void SetUp() override {
    base::HistogramTester histogram_tester;

    // Set up the profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath());
    g_temp_history_dir = temp_dir_.GetPath();
    profile_ = profile_builder.Build();

    if (GetParam() == TestState::kSavingBrowserHistoryDisabled) {
      profile_->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled,
                                       true);
    }

    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildTestHistoryService));

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables before proceeding with the tests and
    // tearing down the temporary directory.
    WaitForDB();

    histogram_tester.ExpectBucketCount(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kSuccess, 1);

    // Set up the local DB connection used for assertions.
    base::FilePath db_file =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Media History"));
    ASSERT_TRUE(db_.Open(db_file));

    // Set up the media history store for OTR.
    otr_service_ = std::make_unique<MediaHistoryKeyedService>(
        profile_->GetOffTheRecordProfile());
  }

  void TearDown() override { WaitForDB(); }

  void WaitForDB() {
    base::RunLoop run_loop;

    MediaHistoryKeyedService::Get(profile_.get())
        ->PostTaskToDBForTest(run_loop.QuitClosure());

    run_loop.Run();
  }

  mojom::MediaHistoryStatsPtr GetStatsSync(MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    mojom::MediaHistoryStatsPtr stats_out;

    service->GetMediaHistoryStats(
        base::BindLambdaForTesting([&](mojom::MediaHistoryStatsPtr stats) {
          stats_out = std::move(stats);
          run_loop.Quit();
        }));

    run_loop.Run();
    return stats_out;
  }

  std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginRowsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryOriginRowPtr> out;

    service->GetOriginRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryOriginRowPtr> rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  std::vector<mojom::MediaHistoryPlaybackRowPtr> GetPlaybackRowsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryPlaybackRowPtr> out;

    service->GetMediaHistoryPlaybackRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryPlaybackRowPtr> rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  std::vector<media_feeds::mojom::MediaFeedPtr> GetMediaFeedsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedPtr> out;

    service->GetMediaFeedsForDebug(base::BindLambdaForTesting(
        [&](std::vector<media_feeds::mojom::MediaFeedPtr> rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  MediaHistoryKeyedService* service() const {
    // If the param is true then we use the OTR service to simulate being in
    // incognito.
    if (GetParam() == TestState::kIncognito)
      return otr_service();

    return MediaHistoryKeyedService::Get(profile_.get());
  }

  MediaHistoryKeyedService* otr_service() const { return otr_service_.get(); }

  bool IsReadOnly() const { return GetParam() != TestState::kNormal; }

 private:
  base::ScopedTempDir temp_dir_;

 protected:
  sql::Database& GetDB() { return db_; }
  content::BrowserTaskEnvironment task_environment_;

 private:
  sql::Database db_;
  std::unique_ptr<MediaHistoryKeyedService> otr_service_;
  std::unique_ptr<TestingProfile> profile_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaHistoryStoreUnitTest,
    testing::Values(TestState::kNormal,
                    TestState::kIncognito,
                    TestState::kSavingBrowserHistoryDisabled));

TEST_P(MediaHistoryStoreUnitTest, CreateDatabaseTables) {
  ASSERT_TRUE(GetDB().DoesTableExist("origin"));
  ASSERT_TRUE(GetDB().DoesTableExist("playback"));
  ASSERT_TRUE(GetDB().DoesTableExist("playbackSession"));
  ASSERT_TRUE(GetDB().DoesTableExist("sessionImage"));
  ASSERT_TRUE(GetDB().DoesTableExist("mediaImage"));
  ASSERT_FALSE(GetDB().DoesTableExist("mediaFeed"));
}

TEST_P(MediaHistoryStoreUnitTest, SavePlayback) {
  base::HistogramTester histogram_tester;

  const auto now_before =
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToJsTime();

  // Create a media player watch time and save it to the playbacks table.
  GURL url("http://google.com/test");
  content::MediaPlayerWatchTime watch_time(url, url.GetOrigin(),
                                           base::TimeDelta::FromSeconds(60),
                                           base::TimeDelta(), true, false);
  service()->SavePlayback(watch_time);
  const auto now_after_a = base::Time::Now().ToJsTime();

  // Save the watch time a second time.
  service()->SavePlayback(watch_time);

  // Wait until the playbacks have finished saving.
  WaitForDB();

  const auto now_after_b = base::Time::Now().ToJsTime();

  // Verify that the playback table contains the expected number of items.
  std::vector<mojom::MediaHistoryPlaybackRowPtr> playbacks =
      GetPlaybackRowsSync(service());

  if (IsReadOnly()) {
    EXPECT_TRUE(playbacks.empty());
  } else {
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
  }

  // Verify that the origin table contains the expected number of items.
  std::vector<mojom::MediaHistoryOriginRowPtr> origins =
      GetOriginRowsSync(service());

  if (IsReadOnly()) {
    EXPECT_TRUE(origins.empty());
  } else {
    EXPECT_EQ(1u, origins.size());
    EXPECT_EQ("http://google.com", origins[0]->origin.Serialize());
    EXPECT_LE(now_before, origins[0]->last_updated_time);
    EXPECT_GE(now_after_b, origins[0]->last_updated_time);
  }

  // The OTR service should have the same data.
  EXPECT_EQ(origins, GetOriginRowsSync(otr_service()));
  EXPECT_EQ(playbacks, GetPlaybackRowsSync(otr_service()));

  histogram_tester.ExpectBucketCount(
      MediaHistoryStore::kPlaybackWriteResultHistogramName,
      MediaHistoryStore::PlaybackWriteResult::kSuccess, IsReadOnly() ? 0 : 2);
}

TEST_P(MediaHistoryStoreUnitTest, GetStats) {
  {
    // Check all the tables are empty.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(0,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);
    EXPECT_EQ(
        0, stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }

  {
    // Create a media player watch time and save it to the playbacks table.
    GURL url("http://google.com/test");
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromMilliseconds(123),
        base::TimeDelta::FromMilliseconds(321), true, false);
    service()->SavePlayback(watch_time);
  }

  {
    // Check the tables have records in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
      EXPECT_EQ(
          0,
          stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
    } else {
      EXPECT_EQ(1,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(1,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
      EXPECT_EQ(
          0,
          stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreUnitTest, UrlShouldBeUniqueForSessions) {
  base::HistogramTester histogram_tester;

  GURL url_a("https://www.google.com");
  GURL url_b("https://www.example.org");

  {
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }

  // Save a couple of sessions on different URLs.
  service()->SavePlaybackSession(url_a, media_session::MediaMetadata(),
                                 base::nullopt,
                                 std::vector<media_session::MediaImage>());
  service()->SavePlaybackSession(url_b, media_session::MediaMetadata(),
                                 base::nullopt,
                                 std::vector<media_session::MediaImage>());

  // Wait until the sessions have finished saving.
  WaitForDB();

  {
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
    } else {
      EXPECT_EQ(2,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);

      sql::Statement s(GetDB().GetUniqueStatement(
          "SELECT id FROM playbackSession WHERE url = ?"));
      s.BindString(0, url_a.spec());
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(1, s.ColumnInt(0));
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }

  // Save a session on the first URL.
  service()->SavePlaybackSession(url_a, media_session::MediaMetadata(),
                                 base::nullopt,
                                 std::vector<media_session::MediaImage>());

  // Wait until the sessions have finished saving.
  WaitForDB();

  {
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
    } else {
      EXPECT_EQ(2,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);

      // The OTR service should have the same data.
      EXPECT_EQ(stats, GetStatsSync(otr_service()));

      // The row for |url_a| should have been replaced so we should have a new
      // ID.
      sql::Statement s(GetDB().GetUniqueStatement(
          "SELECT id FROM playbackSession WHERE url = ?"));
      s.BindString(0, url_a.spec());
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(3, s.ColumnInt(0));
    }
  }

  histogram_tester.ExpectBucketCount(
      MediaHistoryStore::kSessionWriteResultHistogramName,
      MediaHistoryStore::SessionWriteResult::kSuccess, IsReadOnly() ? 0 : 3);
}

TEST_P(MediaHistoryStoreUnitTest, SavePlayback_IncrementAggregateWatchtime) {
  GURL url("http://google.com/test");
  GURL url_alt("http://example.org/test");

  const auto url_now_before = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a watchtime for audio/video for 60 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(60),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record an audio-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), false /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a video-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, false /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  const auto url_now_after = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 60 seconds on a different origin.
    content::MediaPlayerWatchTime watch_time(
        url_alt, url_alt.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  const auto url_alt_after = base::Time::Now().ToJsTime();

  {
    // Check the playbacks were recorded.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    } else {
      EXPECT_EQ(2,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(5,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }

  std::vector<mojom::MediaHistoryOriginRowPtr> origins =
      GetOriginRowsSync(service());

  if (IsReadOnly()) {
    EXPECT_TRUE(origins.empty());
  } else {
    EXPECT_EQ(2u, origins.size());

    EXPECT_EQ("http://google.com", origins[0]->origin.Serialize());
    EXPECT_EQ(base::TimeDelta::FromSeconds(90),
              origins[0]->cached_audio_video_watchtime);
    EXPECT_NEAR(url_now_before, origins[0]->last_updated_time,
                kTimeErrorMargin);
    EXPECT_GE(url_now_after, origins[0]->last_updated_time);
    EXPECT_EQ(origins[0]->cached_audio_video_watchtime,
              origins[0]->actual_audio_video_watchtime);

    EXPECT_EQ("http://example.org", origins[1]->origin.Serialize());
    EXPECT_EQ(base::TimeDelta::FromSeconds(30),
              origins[1]->cached_audio_video_watchtime);
    EXPECT_NEAR(url_now_before, origins[1]->last_updated_time,
                kTimeErrorMargin);
    EXPECT_GE(url_alt_after, origins[1]->last_updated_time);
    EXPECT_EQ(origins[1]->cached_audio_video_watchtime,
              origins[1]->actual_audio_video_watchtime);
  }

  // The OTR service should have the same data.
  EXPECT_EQ(origins, GetOriginRowsSync(otr_service()));
}

TEST_P(MediaHistoryStoreUnitTest, DiscoverMediaFeed_Noop) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  {
    // Check the feeds were not recorded.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());
    EXPECT_FALSE(base::Contains(stats->table_row_counts,
                                MediaHistoryFeedsTable::kTableName));

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }
}

// Runs the tests with the media feeds feature enabled.
class MediaHistoryStoreFeedsTest : public MediaHistoryStoreUnitTest {
 public:
  void SetUp() override {
    features_.InitAndEnableFeature(media::kMediaFeeds);
    MediaHistoryStoreUnitTest::SetUp();
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      MediaHistoryKeyedService* service,
      const int64_t feed_id) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedItemPtr> out;

    service->GetItemsForMediaFeedForDebug(
        feed_id,
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedItemPtr> rows) {
              out = std::move(rows);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  static media_feeds::mojom::ContentRatingPtr CreateRating(
      const std::string& agency,
      const std::string& value) {
    auto rating = media_feeds::mojom::ContentRating::New();
    rating->agency = agency;
    rating->value = value;
    return rating;
  }

  static media_feeds::mojom::IdentifierPtr CreateIdentifier(
      const media_feeds::mojom::Identifier::Type& type,
      const std::string& value) {
    auto identifier = media_feeds::mojom::Identifier::New();
    identifier->type = type;
    identifier->value = value;
    return identifier;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr> GetExpectedItems() {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->name = base::ASCIIToUTF16("The Movie");
      item->type = media_feeds::mojom::MediaFeedItemType::kMovie;
      item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(10));
      item->is_family_friendly = true;
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
      item->genre = base::ASCIIToUTF16("test");
      item->duration = base::TimeDelta::FromSeconds(30);
      item->live = media_feeds::mojom::LiveDetails::New();
      item->live->start_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(20));
      item->live->end_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(30));
      item->shown_count = 3;
      item->clicked = true;
      item->author = media_feeds::mojom::Author::New();
      item->author->name = "Media Site";
      item->author->url = GURL("https://www.example.com");
      item->action = media_feeds::mojom::Action::New();
      item->action->start_time = base::TimeDelta::FromSeconds(3);
      item->action->url = GURL("https://www.example.com");
      item->interaction_counters.emplace(
          media_feeds::mojom::InteractionCounterType::kLike, 10000);
      item->interaction_counters.emplace(
          media_feeds::mojom::InteractionCounterType::kDislike, 20000);
      item->interaction_counters.emplace(
          media_feeds::mojom::InteractionCounterType::kWatch, 30000);
      item->content_ratings.push_back(CreateRating("MPAA", "PG-13"));
      item->content_ratings.push_back(CreateRating("agency", "TEST2"));
      item->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kPartnerId, "TEST1"));
      item->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kTMSId, "TEST2"));
      item->tv_episode = media_feeds::mojom::TVEpisode::New();
      item->tv_episode->name = "TV Episode Name";
      item->tv_episode->season_number = 1;
      item->tv_episode->episode_number = 2;
      item->tv_episode->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kTMSId, "TEST3"));
      item->play_next_candidate = media_feeds::mojom::PlayNextCandidate::New();
      item->play_next_candidate->name = "Next TV Episode Name";
      item->play_next_candidate->season_number = 1;
      item->play_next_candidate->episode_number = 3;
      item->play_next_candidate->duration = base::TimeDelta::FromMinutes(20);
      item->play_next_candidate->action = media_feeds::mojom::Action::New();
      item->play_next_candidate->action->start_time =
          base::TimeDelta::FromSeconds(3);
      item->play_next_candidate->action->url = GURL("https://www.example.com");
      item->play_next_candidate->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kTMSId, "TEST4"));
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnknown;

      {
        media_session::MediaImage image;
        image.src = GURL("https://www.example.org/image1.png");
        item->images.push_back(image);
      }

      {
        media_session::MediaImage image;
        image.src = GURL("https://www.example.org/image2.png");
        image.sizes.push_back(gfx::Size(10, 10));
        item->images.push_back(image);
      }

      items.push_back(std::move(item));
    }

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      item->author = media_feeds::mojom::Author::New();
      item->author->name = "Media Site";
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kSafe;
      items.push_back(std::move(item));
    }

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The Live TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
      item->live = media_feeds::mojom::LiveDetails::New();
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnsafe;
      items.push_back(std::move(item));
    }

    return items;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr>
  GetAltExpectedItems() {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kVideo;
      item->name = base::ASCIIToUTF16("The Video");
      item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(20));
      item->is_family_friendly = false;
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnknown;
      items.push_back(std::move(item));
    }

    return items;
  }

  static std::vector<media_session::MediaImage> GetExpectedLogos() {
    std::vector<media_session::MediaImage> logos;

    {
      media_session::MediaImage image;
      image.src = GURL("https://www.example.org/image1.png");
      image.sizes.push_back(gfx::Size(10, 10));
      logos.push_back(image);
    }

    {
      media_session::MediaImage image;
      image.src = GURL("https://www.example.org/image2.png");
      logos.push_back(image);
    }

    return logos;
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         MediaHistoryStoreFeedsTest,
                         testing::Values(TestState::kNormal,
                                         TestState::kIncognito));

TEST_P(MediaHistoryStoreFeedsTest, CreateDatabaseTables) {
  ASSERT_TRUE(GetDB().DoesTableExist("mediaFeed"));
  ASSERT_TRUE(GetDB().DoesTableExist("mediaFeedItem"));
}

TEST_P(MediaHistoryStoreFeedsTest, DiscoverMediaFeed) {
  GURL url_a("https://www.google.com/feed");
  GURL url_b("https://www.google.co.uk/feed");
  GURL url_c("https://www.google.com/feed2");

  service()->DiscoverMediaFeed(url_a);
  service()->DiscoverMediaFeed(url_b);
  WaitForDB();

  {
    // Check the feeds were recorded.
    std::vector<media_feeds::mojom::MediaFeedPtr> feeds =
        GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(2u, feeds.size());

      EXPECT_EQ(1, feeds[0]->id);
      EXPECT_EQ(url_a, feeds[0]->url);
      EXPECT_FALSE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_FALSE(feeds[0]->cache_expiry_time.has_value());
      EXPECT_EQ(0, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(0, feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(0, feeds[0]->last_fetch_content_types);
      EXPECT_TRUE(feeds[0]->logos.empty());
      EXPECT_TRUE(feeds[0]->display_name.empty());

      EXPECT_EQ(2, feeds[1]->id);
      EXPECT_EQ(url_b, feeds[1]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  service()->DiscoverMediaFeed(url_c);
  WaitForDB();

  {
    // Check the feeds were recorded.
    std::vector<media_feeds::mojom::MediaFeedPtr> feeds =
        GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(2u, feeds.size());

      EXPECT_EQ(2, feeds[0]->id);
      EXPECT_EQ(url_b, feeds[0]->url);
      EXPECT_EQ(3, feeds[1]->id);
      EXPECT_EQ(url_c, feeds[1]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), GetExpectedLogos(), kExpectedDisplayName);
  WaitForDB();

  {
    // The media items should be stored and the feed should be updated.
    auto feeds = GetMediaFeedsSync(service());
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->cache_expiry_time.has_value());
      EXPECT_EQ(kExpectedFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedFetchContentTypes, feeds[0]->last_fetch_content_types);
      EXPECT_EQ(GetExpectedLogos(), feeds[0]->logos);
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);

      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  service()->StoreMediaFeedFetchResult(
      feed_id, GetAltExpectedItems(), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), std::vector<media_session::MediaImage>(),
      kExpectedDisplayName);
  WaitForDB();

  {
    // The media items should be stored and the feed should be updated.
    auto feeds = GetMediaFeedsSync(service());
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->cache_expiry_time.has_value());
      EXPECT_EQ(kExpectedAltFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedAltFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedAltFetchContentTypes,
                feeds[0]->last_fetch_content_types);
      EXPECT_TRUE(feeds[0]->logos.empty());
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);

      EXPECT_EQ(GetAltExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_WithEmpty) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), std::vector<media_session::MediaImage>(),
      std::string());
  WaitForDB();

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  service()->StoreMediaFeedFetchResult(
      feed_id, std::vector<media_feeds::mojom::MediaFeedItemPtr>(),
      media_feeds::mojom::FetchResult::kSuccess, base::Time::Now(),
      std::vector<media_session::MediaImage>(), std::string());
  WaitForDB();

  {
    // There should be no items stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);
    EXPECT_TRUE(items.empty());

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_MultipleFeeds) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  service()->DiscoverMediaFeed(GURL("https://www.google.co.uk/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id_a = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;
  const int feed_id_b = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[1]->id;

  service()->StoreMediaFeedFetchResult(
      feed_id_a, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), std::vector<media_session::MediaImage>(),
      std::string());
  WaitForDB();

  service()->StoreMediaFeedFetchResult(
      feed_id_b, GetAltExpectedItems(),
      media_feeds::mojom::FetchResult::kFailedNetworkError, base::Time::Now(),
      std::vector<media_session::MediaImage>(), std::string());
  WaitForDB();

  {
    // Check the feeds were updated.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(2u, feeds.size());

      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);

      EXPECT_EQ(feed_id_b, feeds[1]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedNetworkError,
                feeds[1]->last_fetch_result);
      EXPECT_EQ(1, feeds[1]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id_a);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
  }

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetAltExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_BadType) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), std::vector<media_session::MediaImage>(),
      std::string());
  WaitForDB();

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  sql::Statement s(
      GetDB().GetUniqueStatement("UPDATE mediaFeedItem SET type = 99"));
  ASSERT_TRUE(s.Run());

  {
    // The items should be skipped because of the invalid type.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);
    EXPECT_TRUE(items.empty());

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, RediscoverMediaFeed) {
  GURL feed_url("https://www.google.com/feed");
  service()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  int feed_id = -1;
  base::Time feed_last_time;

  if (!IsReadOnly()) {
    auto feeds = GetMediaFeedsSync(service());
    feed_id = feeds[0]->id;
    feed_last_time = feeds[0]->last_discovery_time;

    EXPECT_LT(base::Time(), feed_last_time);
    EXPECT_GT(base::Time::Now(), feed_last_time);
    EXPECT_EQ(feed_url, feeds[0]->url);
  }

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), std::vector<media_session::MediaImage>(),
      std::string());
  WaitForDB();

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  // Rediscovering the same feed should not replace the feed.
  service()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  if (!IsReadOnly()) {
    auto feeds = GetMediaFeedsSync(service());

    EXPECT_LE(feed_last_time, feeds[0]->last_discovery_time);
    EXPECT_EQ(feed_id, feeds[0]->id);
    EXPECT_EQ(feed_url, feeds[0]->url);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
              feeds[0]->last_fetch_result);
  }

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  // Finding a new URL should replace the feed.
  GURL new_url("https://www.google.com/feed2");
  service()->DiscoverMediaFeed(new_url);
  WaitForDB();

  if (!IsReadOnly()) {
    auto feeds = GetMediaFeedsSync(service());

    EXPECT_LE(feed_last_time, feeds[0]->last_discovery_time);
    EXPECT_LT(feed_id, feeds[0]->id);
    EXPECT_EQ(new_url, feeds[0]->url);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
              feeds[0]->last_fetch_result);
  }

  {
    // The media items should be deleted.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);
    EXPECT_TRUE(items.empty());

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_IncreaseFailed) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(),
      media_feeds::mojom::FetchResult::kFailedNetworkError, base::Time::Now(),
      GetExpectedLogos(), kExpectedDisplayName);
  WaitForDB();

  {
    // The fetch failed count should have been increased.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedNetworkError,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(1, feeds[0]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(),
      media_feeds::mojom::FetchResult::kFailedBackendError, base::Time::Now(),
      GetExpectedLogos(), kExpectedDisplayName);
  WaitForDB();

  {
    // The fetch failed count should have been increased.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedBackendError,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(2, feeds[0]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), GetExpectedLogos(), kExpectedDisplayName);
  WaitForDB();

  {
    // The fetch failed count should have been reset.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_CheckLogoMax) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  std::vector<media_session::MediaImage> logos;

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image1.png");
    logos.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image2.png");
    logos.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image3.png");
    logos.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image4.png");
    logos.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image5.png");
    logos.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image6.png");
    logos.push_back(image);
  }

  service()->StoreMediaFeedFetchResult(
      feed_id, GetExpectedItems(),
      media_feeds::mojom::FetchResult::kFailedNetworkError, base::Time::Now(),
      logos, kExpectedDisplayName);
  WaitForDB();

  {
    // The feed should have at most 5 logos.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(5u, feeds[0]->logos.size());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_CheckImageMax) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  auto item = media_feeds::mojom::MediaFeedItem::New();
  item->name = base::ASCIIToUTF16("The Movie");
  item->type = media_feeds::mojom::MediaFeedItemType::kMovie;
  item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnknown;

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image1.png");
    item->images.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image2.png");
    item->images.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image3.png");
    item->images.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image4.png");
    item->images.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image5.png");
    item->images.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = GURL("https://www.example.org/image6.png");
    item->images.push_back(image);
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(std::move(item));

  service()->StoreMediaFeedFetchResult(
      feed_id, std::move(items), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), GetExpectedLogos(), kExpectedDisplayName);
  WaitForDB();

  {
    // The item should have at most 5 images.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(5u, items[0]->images.size());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest,
       StoreMediaFeedFetchResult_DefaultSafeSearchResult) {
  service()->DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  auto item = media_feeds::mojom::MediaFeedItem::New();
  item->name = base::ASCIIToUTF16("The Movie");
  item->type = media_feeds::mojom::MediaFeedItemType::kMovie;

  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(std::move(item));

  service()->StoreMediaFeedFetchResult(
      feed_id, std::move(items), media_feeds::mojom::FetchResult::kSuccess,
      base::Time::Now(), GetExpectedLogos(), kExpectedDisplayName);
  WaitForDB();

  {
    // The item should set a default safe search result.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
                items[0]->safe_search_result);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

}  // namespace media_history
