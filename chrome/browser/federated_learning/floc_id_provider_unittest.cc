// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

namespace {

class MockFlocIdProvider : public FlocIdProviderImpl {
 public:
  using FlocIdProviderImpl::FlocIdProviderImpl;

  void NotifyFlocIdUpdated(EventLoggingAction event_logging_action) override {
    ++floc_id_notification_count_;
    FlocIdProviderImpl::NotifyFlocIdUpdated(event_logging_action);
  }

  bool AreThirdPartyCookiesAllowed() override {
    return third_party_cookies_allowed_;
  }

  bool IsSwaaNacAccountEnabled() override { return swaa_nac_account_enabled_; }

  size_t floc_id_notification_count() const {
    return floc_id_notification_count_;
  }

  void set_third_party_cookies_allowed(bool allowed) {
    third_party_cookies_allowed_ = allowed;
  }

  void set_swaa_nac_account_enabled(bool enabled) {
    swaa_nac_account_enabled_ = enabled;
  }

 private:
  size_t floc_id_notification_count_ = 0u;
  bool third_party_cookies_allowed_ = true;
  bool swaa_nac_account_enabled_ = true;
};

}  // namespace

class FlocIdProviderUnitTest : public testing::Test {
 public:
  FlocIdProviderUnitTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~FlocIdProviderUnitTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    test_sync_service_->SetTransportState(
        syncer::SyncService::TransportState::DISABLED);

    fake_user_event_service_ = std::make_unique<syncer::FakeUserEventService>();

    floc_id_provider_ = std::make_unique<MockFlocIdProvider>(
        test_sync_service_.get(), /*cookie_settings=*/nullptr,
        history_service_.get(), fake_user_event_service_.get());

    task_environment_.RunUntilIdle();
  }

  void CheckCanComputeFlocId(
      FlocIdProviderImpl::CanComputeFlocIdCallback callback) {
    floc_id_provider_->CheckCanComputeFlocId(std::move(callback));
  }

  void OnGetRecentlyVisitedURLsCompleted(size_t floc_session_count,
                                         history::QueryResults results) {
    floc_id_provider_->OnGetRecentlyVisitedURLsCompleted(floc_session_count,
                                                         std::move(results));
  }

  FlocId floc_id() const { return floc_id_provider_->floc_id_; }

  void set_floc_id(const FlocId& floc_id) const {
    floc_id_provider_->floc_id_ = floc_id;
  }

  size_t floc_session_count() const {
    return floc_id_provider_->floc_session_count_;
  }

  void set_floc_session_count(size_t count) {
    floc_id_provider_->floc_session_count_ = count;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<syncer::FakeUserEventService> fake_user_event_service_;
  std::unique_ptr<MockFlocIdProvider> floc_id_provider_;

  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(FlocIdProviderUnitTest);
};

TEST_F(FlocIdProviderUnitTest, QualifiedInitialHistory) {
  // Add a history entry with a timestamp exactly 7 days back from now.
  std::string domain = "foo.com";

  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(7);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Expect that the floc session hasn't started, as the floc_id_provider hasn't
  // been notified about state of the sync_service.
  ASSERT_EQ(0u, floc_id_provider_->floc_id_notification_count());
  ASSERT_FALSE(floc_id().IsValid());
  ASSERT_EQ(0u, floc_session_count());

  // Trigger the start of the 1st floc session.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect a floc id update notification.
  ASSERT_EQ(1u, floc_id_provider_->floc_id_notification_count());
  ASSERT_TRUE(floc_id().IsValid());
  ASSERT_EQ(FlocId::CreateFromHistory({domain}).ToDebugHeaderValue(),
            floc_id().ToDebugHeaderValue());
  ASSERT_EQ(1u, floc_session_count());

  // Advance the clock by 1 day. Expect a floc id update notification, as
  // there's no history in the last 7 days so the id has been reset to empty.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  ASSERT_EQ(2u, floc_id_provider_->floc_id_notification_count());
  ASSERT_FALSE(floc_id().IsValid());
  ASSERT_EQ(2u, floc_session_count());
}

TEST_F(FlocIdProviderUnitTest, UnqualifiedInitialHistory) {
  std::string domain = "foo.com";

  // Add a history entry with a timestamp 8 days back from now.
  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(8);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Expect that the floc session hasn't started, as the floc_id_provider hasn't
  // been notified about state of the sync_service.
  ASSERT_EQ(0u, floc_id_provider_->floc_id_notification_count());
  ASSERT_FALSE(floc_id().IsValid());
  ASSERT_EQ(0u, floc_session_count());

  // Trigger the start of the 1st floc session.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect no floc id update notification, as there is no qualified history
  // entry. However, the 1st session should already have started.
  ASSERT_EQ(0u, floc_id_provider_->floc_id_notification_count());
  ASSERT_EQ(1u, floc_session_count());

  // Add a history entry with a timestamp 6 days back from now.
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(6);
  history_service_->AddPage(add_page_args);

  // Advance the clock by 23 hours. Expect no floc id update notification,
  // as the id refresh interval is 24 hours.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(23));

  ASSERT_EQ(0u, floc_id_provider_->floc_id_notification_count());
  ASSERT_EQ(1u, floc_session_count());

  // Advance the clock by 1 hour. Expect a floc id update notification, as the
  // refresh time is reached and there's a valid history entry in the last 7
  // days.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  ASSERT_EQ(1u, floc_id_provider_->floc_id_notification_count());
  ASSERT_TRUE(floc_id().IsValid());
  ASSERT_EQ(FlocId::CreateFromHistory({domain}).ToDebugHeaderValue(),
            floc_id().ToDebugHeaderValue());
  ASSERT_EQ(2u, floc_session_count());
}

TEST_F(FlocIdProviderUnitTest, CheckCanComputeFlocId_Success) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { ASSERT_TRUE(can_compute_floc); });

  CheckCanComputeFlocId(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest, CheckCanComputeFlocId_Failure_SyncDisabled) {
  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { ASSERT_FALSE(can_compute_floc); });

  CheckCanComputeFlocId(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest,
       CheckCanComputeFlocId_Failure_BlockThirdPartyCookies) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  floc_id_provider_->set_third_party_cookies_allowed(false);

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { ASSERT_FALSE(can_compute_floc); });

  CheckCanComputeFlocId(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest,
       CheckCanComputeFlocId_Failure_SwaaNacAccountDisabled) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  floc_id_provider_->set_swaa_nac_account_enabled(false);

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { ASSERT_FALSE(can_compute_floc); });

  CheckCanComputeFlocId(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest, EventLogging) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFlocIdComputedEventLogging);

  set_floc_session_count(1u);
  set_floc_id(FlocId(12345ULL));
  floc_id_provider_->NotifyFlocIdUpdated(
      FlocIdProviderImpl::EventLoggingAction::kAllow);

  ASSERT_EQ(1u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics1 =
      fake_user_event_service_->GetRecordedUserEvents()[0];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics1.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event1 =
      specifics1.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::NEW,
            event1.event_trigger());
  EXPECT_EQ(12345ULL, event1.floc_id());

  set_floc_session_count(2u);
  set_floc_id(FlocId(999ULL));
  floc_id_provider_->NotifyFlocIdUpdated(
      FlocIdProviderImpl::EventLoggingAction::kAllow);

  ASSERT_EQ(2u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics2 =
      fake_user_event_service_->GetRecordedUserEvents()[1];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics2.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event2 =
      specifics2.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::REFRESHED,
            event2.event_trigger());
  EXPECT_EQ(999ULL, event2.floc_id());
}

TEST_F(FlocIdProviderUnitTest, HistoryEntriesWithPrivateIP) {
  history::QueryResults query_results;
  query_results.SetURLResults(
      {history::URLResult(GURL("https://a.test"),
                          base::Time::Now() - base::TimeDelta::FromDays(1))});

  set_floc_session_count(1u);
  OnGetRecentlyVisitedURLsCompleted(1u, std::move(query_results));

  ASSERT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, MultipleHistoryEntries) {
  base::Time time = base::Time::Now() - base::TimeDelta::FromDays(1);

  history::URLResult url_result_a(GURL("https://a.test"), time);
  url_result_a.set_publicly_routable(true);

  history::URLResult url_result_b(GURL("https://b.test"), time);
  url_result_b.set_publicly_routable(true);

  history::URLResult url_result_c(GURL("https://c.test"), time);

  std::vector<history::URLResult> url_results{url_result_a, url_result_b,
                                              url_result_c};

  history::QueryResults query_results;
  query_results.SetURLResults(std::move(url_results));

  set_floc_session_count(1u);
  OnGetRecentlyVisitedURLsCompleted(1u, std::move(query_results));

  ASSERT_EQ(
      FlocId::CreateFromHistory({"a.test", "b.test"}).ToDebugHeaderValue(),
      floc_id().ToDebugHeaderValue());
}

TEST_F(FlocIdProviderUnitTest, TurnSyncOffAndOn) {
  std::string domain = "foo.com";

  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(1);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Trigger the start of the 1st floc session.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect a floc id update notification.
  ASSERT_EQ(1u, floc_id_provider_->floc_id_notification_count());
  ASSERT_EQ(FlocId::CreateFromHistory({domain}).ToDebugHeaderValue(),
            floc_id().ToDebugHeaderValue());
  ASSERT_EQ(1u, floc_session_count());

  // Turn off sync.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::DISABLED);

  // Advance the clock by 1 day. Expect a floc id update notification, as
  // the sync was turned off so the id has been reset to empty.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  ASSERT_EQ(2u, floc_id_provider_->floc_id_notification_count());
  ASSERT_FALSE(floc_id().IsValid());
  ASSERT_EQ(2u, floc_session_count());

  // Turn on sync.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  // Advance the clock by 1 day. Expect a floc id update notification and a
  // valid floc id.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  ASSERT_EQ(3u, floc_id_provider_->floc_id_notification_count());
  ASSERT_EQ(FlocId::CreateFromHistory({domain}).ToDebugHeaderValue(),
            floc_id().ToDebugHeaderValue());
  ASSERT_EQ(3u, floc_session_count());
}

}  // namespace federated_learning
