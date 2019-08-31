// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/sync_wifi/pending_network_configuration_tracker_impl.h"
#include "chromeos/components/sync_wifi/test_specifics_generator.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFredSsid[] = "Fred";
const char kMangoSsid[] = "Mango";
const char kChangeGuid1[] = "change-1";
const char kChangeGuid2[] = "change-2";

const char kPendingNetworkConfigurationsPref[] =
    "sync_wifi.pending_network_configuration_updates";
const char kChangeGuidKey[] = "ChangeGuid";

}  // namespace

namespace sync_wifi {

class PendingNetworkConfigurationTrackerImplTest : public testing::Test {
 public:
  PendingNetworkConfigurationTrackerImplTest() = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();

    PendingNetworkConfigurationTrackerImpl::RegisterProfilePrefs(
        test_pref_service_->registry());
    tracker_ = std::make_unique<PendingNetworkConfigurationTrackerImpl>(
        test_pref_service_.get());
  }

  PendingNetworkConfigurationTrackerImpl* tracker() { return tracker_.get(); }

  const base::Value* GetPref() const {
    return test_pref_service_.get()->GetUserPref(
        kPendingNetworkConfigurationsPref);
  }

  bool DoesPrefContainPendingUpdate(const std::string& ssid,
                                    const std::string& update_guid) const {
    const base::Value* dict = GetPref()->FindPath(ssid);
    if (!dict)
      return false;

    const std::string* found_guid = dict->FindStringKey(kChangeGuidKey);
    return found_guid && *found_guid == update_guid;
  }

  void AssertTrackerHasMatchingUpdate(
      const std::string& update_guid,
      const std::string& ssid,
      int completed_attempts = 0,
      const base::Optional<sync_pb::WifiConfigurationSpecificsData> specifics =
          base::nullopt) {
    base::Optional<PendingNetworkConfigurationUpdate> update =
        tracker()->GetPendingUpdate(update_guid, ssid);
    ASSERT_TRUE(update);
    ASSERT_EQ(ssid, update->ssid());
    ASSERT_EQ(completed_attempts, update->completed_attempts());
    std::string serialized_specifics_wants;
    std::string serialized_specifics_has;
    if (specifics)
      specifics->SerializeToString(&serialized_specifics_wants);
    if (update->specifics())
      update->specifics()->SerializeToString(&serialized_specifics_has);
    ASSERT_EQ(serialized_specifics_wants, serialized_specifics_has);
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<PendingNetworkConfigurationTrackerImpl> tracker_;
  DISALLOW_COPY_AND_ASSIGN(PendingNetworkConfigurationTrackerImplTest);
};

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestMarkComplete) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  AssertTrackerHasMatchingUpdate(kChangeGuid1, kFredSsid);
  EXPECT_EQ(1u, GetPref()->DictSize());
  EXPECT_TRUE(DoesPrefContainPendingUpdate(kFredSsid, kChangeGuid1));
  tracker()->MarkComplete(kChangeGuid1, kFredSsid);
  EXPECT_FALSE(tracker()->GetPendingUpdate(kChangeGuid1, kFredSsid));
  EXPECT_EQ(0u, GetPref()->DictSize());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestTwoChangesSameNetwork) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  tracker()->IncrementCompletedAttempts(kChangeGuid1, kFredSsid);
  AssertTrackerHasMatchingUpdate(kChangeGuid1, kFredSsid,
                                 /*completed_attempts=*/1);
  EXPECT_EQ(1u, GetPref()->DictSize());
  EXPECT_EQ(1, tracker()
                   ->GetPendingUpdate(kChangeGuid1, kFredSsid)
                   ->completed_attempts());

  tracker()->TrackPendingUpdate(kChangeGuid2, kFredSsid,
                                /*specifics=*/base::nullopt);
  EXPECT_FALSE(tracker()->GetPendingUpdate(kChangeGuid1, kFredSsid));
  AssertTrackerHasMatchingUpdate(kChangeGuid2, kFredSsid);
  EXPECT_EQ(0, tracker()
                   ->GetPendingUpdate(kChangeGuid2, kFredSsid)
                   ->completed_attempts());
  EXPECT_EQ(1u, GetPref()->DictSize());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest,
       TestTwoChangesDifferentNetworks) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  AssertTrackerHasMatchingUpdate(kChangeGuid1, kFredSsid);
  EXPECT_TRUE(DoesPrefContainPendingUpdate(kFredSsid, kChangeGuid1));
  EXPECT_EQ(1u, GetPref()->DictSize());
  tracker()->TrackPendingUpdate(kChangeGuid2, kMangoSsid,
                                /*specifics=*/base::nullopt);
  AssertTrackerHasMatchingUpdate(kChangeGuid1, kFredSsid);
  AssertTrackerHasMatchingUpdate(kChangeGuid2, kMangoSsid);
  EXPECT_TRUE(DoesPrefContainPendingUpdate(kFredSsid, kChangeGuid1));
  EXPECT_TRUE(DoesPrefContainPendingUpdate(kMangoSsid, kChangeGuid2));
  EXPECT_EQ(2u, GetPref()->DictSize());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestGetPendingUpdates) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  tracker()->TrackPendingUpdate(kChangeGuid2, kMangoSsid,
                                /*specifics=*/base::nullopt);
  std::vector<PendingNetworkConfigurationUpdate> list =
      tracker()->GetPendingUpdates();
  EXPECT_EQ(2u, list.size());
  EXPECT_EQ(kChangeGuid1, list[0].change_guid());
  EXPECT_EQ(kFredSsid, list[0].ssid());
  EXPECT_EQ(kChangeGuid2, list[1].change_guid());
  EXPECT_EQ(kMangoSsid, list[1].ssid());

  tracker()->MarkComplete(kChangeGuid1, kFredSsid);
  list = tracker()->GetPendingUpdates();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(kChangeGuid2, list[0].change_guid());
  EXPECT_EQ(kMangoSsid, list[0].ssid());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestGetPendingUpdate) {
  sync_pb::WifiConfigurationSpecificsData specifics =
      CreateSpecifics(kFredSsid);
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid, specifics);

  AssertTrackerHasMatchingUpdate(kChangeGuid1, kFredSsid,
                                 /*completed_attempts=*/0, specifics);

  EXPECT_FALSE(tracker()->GetPendingUpdate(kChangeGuid2, kMangoSsid));
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestRetryCounting) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  AssertTrackerHasMatchingUpdate(kChangeGuid1, kFredSsid);
  EXPECT_EQ(1u, GetPref()->DictSize());
  EXPECT_EQ(0, tracker()
                   ->GetPendingUpdate(kChangeGuid1, kFredSsid)
                   ->completed_attempts());
  tracker()->IncrementCompletedAttempts(kChangeGuid1, kFredSsid);
  tracker()->IncrementCompletedAttempts(kChangeGuid1, kFredSsid);
  tracker()->IncrementCompletedAttempts(kChangeGuid1, kFredSsid);
  EXPECT_EQ(3, tracker()
                   ->GetPendingUpdate(kChangeGuid1, kFredSsid)
                   ->completed_attempts());
  tracker()->IncrementCompletedAttempts(kChangeGuid1, kFredSsid);
  tracker()->IncrementCompletedAttempts(kChangeGuid1, kFredSsid);
  EXPECT_EQ(5, tracker()
                   ->GetPendingUpdate(kChangeGuid1, kFredSsid)
                   ->completed_attempts());
}

}  // namespace sync_wifi
