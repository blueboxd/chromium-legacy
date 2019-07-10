// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_device_registration.h"

#include <map>
#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/fake_local_device_info_provider.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace gcm;
using namespace instance_id;
using namespace testing;

namespace {
const char kAppID[] = "test_app_id";
const char kFCMToken[] = "test_fcm_token";
const char kFCMToken2[] = "test_fcm_token_2";
const char kDevicep256dh[] = "test_p256_dh";
const char kDeviceAuthSecret[] = "test_auth_secret";

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID, InstanceID*(const std::string& app_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class FakeInstanceID : public InstanceID {
 public:
  FakeInstanceID() : InstanceID(kAppID, /*gcm_driver = */ nullptr) {}
  ~FakeInstanceID() override = default;

  void GetID(const GetIDCallback& callback) override { NOTIMPLEMENTED(); }

  void GetCreationTime(const GetCreationTimeCallback& callback) override {
    NOTIMPLEMENTED();
  }

  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                bool is_lazy,
                GetTokenCallback callback) override {
    std::move(callback).Run(fcm_token_, result_);
  }

  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     const ValidateTokenCallback& callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteIDImpl(DeleteIDCallback callback) override { NOTIMPLEMENTED(); }

  void SetFCMResult(InstanceID::Result result) { result_ = result; }

  void SetFCMToken(std::string fcm_token) { fcm_token_ = std::move(fcm_token); }

 private:
  InstanceID::Result result_;
  std::string fcm_token_;
};

class FakeEncryptionGCMDriver : public FakeGCMDriver {
 public:
  FakeEncryptionGCMDriver() = default;
  ~FakeEncryptionGCMDriver() override = default;

  void GetEncryptionInfo(const std::string& app_id,
                         GetEncryptionInfoCallback callback) override {
    std::move(callback).Run(kDevicep256dh, kDeviceAuthSecret);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeEncryptionGCMDriver);
};

class SharingDeviceRegistrationTest : public testing::Test {
 public:
  SharingDeviceRegistrationTest()
      : sync_prefs_(&prefs_),
        vapid_key_manager_(&sync_prefs_),
        sharing_device_registration_(&sync_prefs_,
                                     &mock_instance_id_driver_,
                                     &vapid_key_manager_,
                                     &fake_encryption_gcm_driver_,
                                     &fake_local_device_info_provider_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  void SetUp() {
    ON_CALL(mock_instance_id_driver_, GetInstanceID(_))
        .WillByDefault(testing::Return(&fake_instance_id_));
  }

  void RegisterDeviceSync() {
    base::RunLoop run_loop;
    sharing_device_registration_.RegisterDevice(
        base::BindLambdaForTesting([&](SharingDeviceRegistration::Result r) {
          result_ = r;
          devices_ = sync_prefs_.GetSyncedDevices();
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void SetInstanceIDFCMResult(InstanceID::Result result) {
    fake_instance_id_.SetFCMResult(result);
  }

  void SetInstanceIDFCMToken(std::string fcm_token) {
    fake_instance_id_.SetFCMToken(std::move(fcm_token));
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeEncryptionGCMDriver fake_encryption_gcm_driver_;
  NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  FakeInstanceID fake_instance_id_;

  SharingSyncPreference sync_prefs_;
  VapidKeyManager vapid_key_manager_;
  SharingDeviceRegistration sharing_device_registration_;

  // callback results
  std::map<std::string, SharingSyncPreference::Device> devices_;
  SharingDeviceRegistration::Result result_;
};

}  // namespace

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_Success) {
  SetInstanceIDFCMResult(InstanceID::Result::SUCCESS);
  SetInstanceIDFCMToken(kFCMToken);

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistration::Result::SUCCESS, result_);
  std::string guid =
      fake_local_device_info_provider_.GetLocalDeviceInfo()->guid();
  auto it = devices_.find(guid);

  ASSERT_NE(devices_.end(), it);
  SharingSyncPreference::Device device(std::move(it->second));
  EXPECT_EQ(kDeviceAuthSecret, device.auth_secret);
  EXPECT_EQ(kDevicep256dh, device.p256dh);
  EXPECT_EQ(kFCMToken, device.fcm_token);
  EXPECT_EQ(sharing_device_registration_.GetDeviceCapabilities(),
            device.capabilities);

  // Remove VAPId key to force a re-register, which will return a different FCM
  // token.
  prefs_.RemoveUserPref("sharing.vapid_key");
  SetInstanceIDFCMToken(kFCMToken2);

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistration::Result::SUCCESS, result_);

  // Device should be re-registered with the new FCM token.
  it = devices_.find(guid);
  ASSERT_NE(devices_.end(), it);
  EXPECT_EQ(kFCMToken2, it->second.fcm_token);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_VapidKeysUnchanged) {
  SetInstanceIDFCMResult(InstanceID::Result::SUCCESS);

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistration::Result::SUCCESS, result_);

  // Set instance ID result to error. InstanceID shouldn't be invoked.
  SetInstanceIDFCMResult(InstanceID::Result::UNKNOWN_ERROR);
  // Register device again without changing VAPID keys.
  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistration::Result::SUCCESS, result_);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_NetworkError) {
  SetInstanceIDFCMResult(InstanceID::Result::NETWORK_ERROR);

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistration::Result::TRANSIENT_ERROR, result_);
  std::string guid =
      fake_local_device_info_provider_.GetLocalDeviceInfo()->guid();
  auto it = devices_.find(guid);
  EXPECT_EQ(devices_.end(), it);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_FatalError) {
  SetInstanceIDFCMResult(InstanceID::Result::DISABLED);

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistration::Result::FATAL_ERROR, result_);
  std::string guid =
      fake_local_device_info_provider_.GetLocalDeviceInfo()->guid();
  auto it = devices_.find(guid);
  EXPECT_EQ(devices_.end(), it);
}
