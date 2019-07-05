// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/sharing/fake_local_device_info_provider.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedTaskEnvironment;
using namespace instance_id;
using namespace testing;

namespace {

constexpr int kNoCapabilities =
    static_cast<int>(SharingDeviceCapability::kNone);
const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kFcmToken[] = "fcm_token";
const int kTtlSeconds = 10;

class MockGCMDriver : public gcm::FakeGCMDriver {
 public:
  MockGCMDriver() {}
  ~MockGCMDriver() override {}

  MOCK_METHOD8(SendWebPushMessage,
               void(const std::string& app_id,
                    const std::string& authorized_entity,
                    const std::string& p256dh,
                    const std::string& auth_secret,
                    const std::string& fcm_token,
                    crypto::ECPrivateKey* vapid_key,
                    gcm::WebPushMessage message,
                    gcm::GCMDriver::SendWebPushMessageCallback callback));
};

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class SharingServiceTest : public testing::Test {
 public:
  SharingServiceTest() {
    sync_prefs_ = new SharingSyncPreference(&prefs_);
    sharing_device_registration_ = new SharingDeviceRegistration(
        sync_prefs_, &mock_instance_id_driver_, vapid_key_manager_,
        &mock_gcm_driver_, &fake_local_device_info_provider_);
    vapid_key_manager_ = new VapidKeyManager(sync_prefs_);
    fcm_sender_ = new SharingFCMSender(&mock_gcm_driver_,
                                       &fake_local_device_info_provider_,
                                       sync_prefs_, vapid_key_manager_);
    fcm_handler_ = new SharingFCMHandler(&mock_gcm_driver_, fcm_sender_);
    sharing_service_ = std::make_unique<SharingService>(
        base::WrapUnique(sync_prefs_), base::WrapUnique(vapid_key_manager_),
        base::WrapUnique(sharing_device_registration_),
        base::WrapUnique(fcm_sender_), base::WrapUnique(fcm_handler_),
        &device_info_tracker_, &fake_sync_service_);
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }
  void OnMessageSent(base::Optional<std::string> message_id) {}

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
    return SharingSyncPreference::Device(kFcmToken, kP256dh, kAuthSecret,
                                         kNoCapabilities);
  }

  ScopedTaskEnvironment scoped_task_environment_{
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};

  syncer::FakeDeviceInfoTracker device_info_tracker_;
  FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  syncer::FakeSyncService fake_sync_service_;

  NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  NiceMock<MockGCMDriver> mock_gcm_driver_;

  SharingSyncPreference* sync_prefs_;
  VapidKeyManager* vapid_key_manager_;
  SharingDeviceRegistration* sharing_device_registration_;
  SharingFCMSender* fcm_sender_;
  SharingFCMHandler* fcm_handler_;
  std::unique_ptr<SharingService> sharing_service_ = nullptr;

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

TEST_F(SharingServiceTest, SendMessageToDevice) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info = CreateFakeDeviceInfo(id);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  EXPECT_CALL(mock_gcm_driver_,
              SendWebPushMessage(_, _, Eq(kP256dh), Eq(kAuthSecret),
                                 Eq(kFcmToken), _, _, _))
      .Times(1);
  sharing_service_->SendMessageToDevice(
      id, base::TimeDelta::FromSeconds(kTtlSeconds),
      chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));
}

// TODO(himanshujaju) Add tests for RegisterDevice
