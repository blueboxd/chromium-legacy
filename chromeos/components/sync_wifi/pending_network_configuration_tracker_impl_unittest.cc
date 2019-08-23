// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/pending_network_configuration_tracker_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
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

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<PendingNetworkConfigurationTrackerImpl> tracker_;
  DISALLOW_COPY_AND_ASSIGN(PendingNetworkConfigurationTrackerImplTest);
};

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestMarkComplete) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  EXPECT_TRUE(tracker()->IsChangeTracked(kChangeGuid1, kFredSsid));
  EXPECT_EQ(1u, GetPref()->DictSize());
  EXPECT_TRUE(DoesPrefContainPendingUpdate(kFredSsid, kChangeGuid1));
  tracker()->MarkComplete(kChangeGuid1, kFredSsid);
  EXPECT_FALSE(tracker()->IsChangeTracked(kChangeGuid1, kFredSsid));
  EXPECT_EQ(0u, GetPref()->DictSize());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestTwoChangesSameNetwork) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  EXPECT_TRUE(tracker()->IsChangeTracked(kChangeGuid1, kFredSsid));
  EXPECT_EQ(1u, GetPref()->DictSize());
  tracker()->TrackPendingUpdate(kChangeGuid2, kFredSsid,
                                /*specifics=*/base::nullopt);
  EXPECT_FALSE(tracker()->IsChangeTracked(kChangeGuid1, kFredSsid));
  EXPECT_TRUE(tracker()->IsChangeTracked(kChangeGuid2, kFredSsid));
  EXPECT_EQ(1u, GetPref()->DictSize());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest,
       TestTwoChangesDifferentNetworks) {
  tracker()->TrackPendingUpdate(kChangeGuid1, kFredSsid,
                                /*specifics=*/base::nullopt);
  EXPECT_TRUE(tracker()->IsChangeTracked(kChangeGuid1, kFredSsid));
  EXPECT_TRUE(DoesPrefContainPendingUpdate(kFredSsid, kChangeGuid1));
  EXPECT_EQ(1u, GetPref()->DictSize());
  tracker()->TrackPendingUpdate(kChangeGuid2, kMangoSsid,
                                /*specifics=*/base::nullopt);
  EXPECT_TRUE(tracker()->IsChangeTracked(kChangeGuid1, kFredSsid));
  EXPECT_TRUE(tracker()->IsChangeTracked(kChangeGuid2, kMangoSsid));
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

}  // namespace sync_wifi
