// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedTaskEnvironment;

namespace {

constexpr int kNoCapabilities =
    static_cast<int>(SharingDeviceCapability::kNone);

class SharingServiceTest : public testing::Test {
 public:
  SharingServiceTest() {
    sync_prefs_ = new SharingSyncPreference(&prefs_);
    sharing_service_ = std::make_unique<SharingService>(
        base::WrapUnique(sync_prefs_), &device_info_tracker_);
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

 protected:
  static std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
      const std::string& id) {
    return std::make_unique<syncer::DeviceInfo>(
        id, "name", "chrome_version", "user_agent",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
        /* last_updated_timestamp= */ base::Time::Now(),
        /* send_tab_to_self_receiving_enabled= */ false);
  }

  static SharingSyncPreference::Device CreateFakeSyncDevice() {
    return SharingSyncPreference::Device("fcm_token", "p256dh", "auth_token",
                                         kNoCapabilities);
  }

  ScopedTaskEnvironment scoped_task_environment_{
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};

  syncer::FakeDeviceInfoTracker device_info_tracker_;
  SharingSyncPreference* sync_prefs_ = nullptr;
  std::unique_ptr<SharingService> sharing_service_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

}  // namespace

TEST_F(SharingServiceTest, GetDeviceCandidates_Empty) {
  std::vector<SharingDeviceInfo> candidates =
      sharing_service_->GetDeviceCandidates(kNoCapabilities);
  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_NoSynced) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info = CreateFakeDeviceInfo(id);
  device_info_tracker_.Add(device_info.get());

  std::vector<SharingDeviceInfo> candidates =
      sharing_service_->GetDeviceCandidates(kNoCapabilities);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_NoTracked) {
  sync_prefs_->SetSyncDevice(base::GenerateGUID(), CreateFakeSyncDevice());

  std::vector<SharingDeviceInfo> candidates =
      sharing_service_->GetDeviceCandidates(kNoCapabilities);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_SyncedAndTracked) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info = CreateFakeDeviceInfo(id);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  std::vector<SharingDeviceInfo> candidates =
      sharing_service_->GetDeviceCandidates(kNoCapabilities);

  ASSERT_EQ(1u, candidates.size());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_Expired) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info = CreateFakeDeviceInfo(id);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  // Forward time until device expires.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromDays(10));

  std::vector<SharingDeviceInfo> candidates =
      sharing_service_->GetDeviceCandidates(kNoCapabilities);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_MissingRequirements) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info = CreateFakeDeviceInfo(id);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  // Require all capabilities.
  std::vector<SharingDeviceInfo> candidates =
      sharing_service_->GetDeviceCandidates(-1);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_DuplicateDeviceNames) {
  // Add first device.
  std::string id1 = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info_1 = CreateFakeDeviceInfo(id1);
  device_info_tracker_.Add(device_info_1.get());
  sync_prefs_->SetSyncDevice(id1, CreateFakeSyncDevice());

  // Advance time for a bit to create a newer device.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  // Add second device.
  std::string id2 = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info_2 = CreateFakeDeviceInfo(id2);
  device_info_tracker_.Add(device_info_2.get());
  sync_prefs_->SetSyncDevice(id2, CreateFakeSyncDevice());

  std::vector<SharingDeviceInfo> candidates =
      sharing_service_->GetDeviceCandidates(kNoCapabilities);

  ASSERT_EQ(1u, candidates.size());
  EXPECT_EQ(id2, candidates[0].guid());
}
