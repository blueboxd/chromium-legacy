// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NearbySharingServiceImplTest : public testing::Test {
 public:
  NearbySharingServiceImplTest() {
    RegisterNearbySharingPrefs(prefs_.registry());
  }

  ~NearbySharingServiceImplTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  std::unique_ptr<NearbySharingServiceImpl> CreateService(
      const std::string& profile_name) {
    Profile* profile = profile_manager_.CreateTestingProfile(profile_name);
    fake_nearby_connections_manager_ = new FakeNearbyConnectionsManager();
    return std::make_unique<NearbySharingServiceImpl>(
        &prefs_, profile, base::WrapUnique(fake_nearby_connections_manager_));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeNearbyConnectionsManager* fake_nearby_connections_manager_;
};

}  // namespace

TEST_F(NearbySharingServiceImplTest, AddsNearbyProcessObserver) {
  std::unique_ptr<NearbySharingServiceImpl> service = CreateService("name");

  NearbyProcessManager& manager = NearbyProcessManager::GetInstance();
  EXPECT_TRUE(manager.observers_.HasObserver(service.get()));
}

TEST_F(NearbySharingServiceImplTest, RemovesNearbyProcessObserver) {
  std::unique_ptr<NearbySharingServiceImpl> service = CreateService("name");
  service.reset();

  NearbyProcessManager& manager = NearbyProcessManager::GetInstance();
  EXPECT_FALSE(manager.observers_.might_have_observers());
}

TEST_F(NearbySharingServiceImplTest, DisableNearbyShutdownConnections) {
  std::unique_ptr<NearbySharingServiceImpl> service = CreateService("name");
  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);

  EXPECT_TRUE(fake_nearby_connections_manager_->IsShutdown());
}
