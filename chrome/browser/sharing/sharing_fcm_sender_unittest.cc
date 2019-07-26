// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include <memory>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/guid.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using SharingMessage = chrome_browser_sharing::SharingMessage;
using namespace testing;

namespace {

const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kFcmToken[] = "fcm_token";
constexpr int kNoCapabilities =
    static_cast<int>(SharingDeviceCapability::kNone);
const char kSenderGuid[] = "test_sender_guid";
const char kAuthorizedEntity[] = "authorized_entity";
const int kTtlSeconds = 10;

class MockGCMDriver : public gcm::FakeGCMDriver {
 public:
  MockGCMDriver() {}
  ~MockGCMDriver() override {}

  MOCK_METHOD8(SendWebPushMessage,
               void(const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    crypto::ECPrivateKey*,
                    gcm::WebPushMessage,
                    SendWebPushMessageCallback));
};

class FakeLocalDeviceInfoProvider : public syncer::LocalDeviceInfoProvider {
 public:
  FakeLocalDeviceInfoProvider()
      : local_device_info_(kSenderGuid,
                           "name",
                           "chrome_version",
                           "user_agent",
                           sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                           "device_id",
                           base::Time::Now(),
                           false) {}
  ~FakeLocalDeviceInfoProvider() override {}

  version_info::Channel GetChannel() const override {
    return version_info::Channel::UNKNOWN;
  }

  const syncer::DeviceInfo* GetLocalDeviceInfo() const override {
    return ready_ ? &local_device_info_ : nullptr;
  }

  std::unique_ptr<syncer::LocalDeviceInfoProvider::Subscription>
  RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override {
    return callback_list_.Add(callback);
  }

  void SetReady(bool ready) {
    bool got_ready = !ready_ && ready;
    ready_ = ready;
    if (got_ready)
      callback_list_.Notify();
  }

 private:
  syncer::DeviceInfo local_device_info_;
  bool ready_ = true;
  base::CallbackList<void(void)> callback_list_;
};

class MockVapidKeyManager : public VapidKeyManager {
 public:
  MockVapidKeyManager() : VapidKeyManager(nullptr) {}
  ~MockVapidKeyManager() {}

  MOCK_METHOD0(GetOrCreateKey, crypto::ECPrivateKey*());
};

class SharingFCMSenderTest : public Test {
 public:
  void OnMessageSent(base::Optional<std::string> message_id) {}

 protected:
  SharingFCMSenderTest() {
    // TODO: Used fake GCMDriver
    sync_prefs_ = std::make_unique<SharingSyncPreference>(&prefs_);
    sharing_fcm_sender_ = std::make_unique<SharingFCMSender>(
        &mock_gcm_driver_, &local_device_info_provider_, sync_prefs_.get(),
        &vapid_key_manager_);
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  SharingSyncPreference::Device CreateFakeSyncDevice() {
    return SharingSyncPreference::Device(kFcmToken, kP256dh, kAuthSecret,
                                         kNoCapabilities);
  }

  std::unique_ptr<SharingSyncPreference> sync_prefs_;
  std::unique_ptr<SharingFCMSender> sharing_fcm_sender_;
  NiceMock<MockGCMDriver> mock_gcm_driver_;
  NiceMock<MockVapidKeyManager> vapid_key_manager_;
  FakeLocalDeviceInfoProvider local_device_info_provider_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

}  // namespace

MATCHER(WebPushMessageMatcher, "") {
  SharingMessage sharing_message;
  sharing_message.ParseFromString(arg.payload);
  return sharing_message.sender_guid() == kSenderGuid &&
         arg.time_to_live == kTtlSeconds &&
         arg.urgency == gcm::WebPushMessage::Urgency::kHigh;
}

TEST_F(SharingFCMSenderTest, SendMessageToDevice) {
  std::string guid = base::GenerateGUID();
  sync_prefs_->SetSyncDevice(guid, CreateFakeSyncDevice());
  sync_prefs_->SetFCMRegistration({kAuthorizedEntity, "", base::Time::Now()});

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(Return(vapid_key.get()));

  SharingMessage sharing_message;
  gcm::WebPushMessage web_push_message;
  web_push_message.time_to_live = kTtlSeconds;
  sharing_message.SerializeToString(&web_push_message.payload);

  EXPECT_CALL(
      mock_gcm_driver_,
      SendWebPushMessage(Eq(kSharingFCMAppID), Eq(kAuthorizedEntity),
                         Eq(kP256dh), Eq(kAuthSecret), Eq(kFcmToken),
                         Eq(vapid_key.get()), WebPushMessageMatcher(), _));

  sharing_fcm_sender_->SendMessageToDevice(
      guid, base::TimeDelta::FromSeconds(kTtlSeconds), SharingMessage(),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this)));
}

TEST_F(SharingFCMSenderTest, SendMessageBeforeLocalDeviceInfoReady) {
  std::string guid = base::GenerateGUID();
  sync_prefs_->SetSyncDevice(guid, CreateFakeSyncDevice());
  sync_prefs_->SetFCMRegistration({kAuthorizedEntity, "", base::Time::Now()});

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(Return(vapid_key.get()));

  SharingMessage sharing_message;
  gcm::WebPushMessage web_push_message;
  web_push_message.time_to_live = kTtlSeconds;
  sharing_message.SerializeToString(&web_push_message.payload);

  local_device_info_provider_.SetReady(false);

  EXPECT_CALL(mock_gcm_driver_, SendWebPushMessage(_, _, _, _, _, _, _, _))
      .Times(0);

  sharing_fcm_sender_->SendMessageToDevice(
      guid, base::TimeDelta::FromSeconds(kTtlSeconds), SharingMessage(),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_CALL(
      mock_gcm_driver_,
      SendWebPushMessage(Eq(kSharingFCMAppID), Eq(kAuthorizedEntity),
                         Eq(kP256dh), Eq(kAuthSecret), Eq(kFcmToken),
                         Eq(vapid_key.get()), WebPushMessageMatcher(), _));

  local_device_info_provider_.SetReady(true);
}
