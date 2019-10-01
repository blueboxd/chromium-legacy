// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include <memory>

#include "base/base64.h"
#include "base/callback_list.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Device = SharingSyncPreference::Device;
using RecipientInfo = chrome_browser_sharing::RecipientInfo;
using SharingMessage = chrome_browser_sharing::SharingMessage;
using namespace testing;

namespace {

const char kMessageId[] = "message_id";
const char kFcmToken[] = "fcm_token";
const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const std::set<sync_pb::SharingSpecificFields::EnabledFeatures>
    kNoFeaturesEnabled;
const char kSenderGuid[] = "test_sender_guid";
const char kSenderFcmToken[] = "sender_fcm_token";
const char kSenderP256dh[] = "sender_p256dh";
const char kSenderAuthSecret[] = "sender_auth_secret";
const char kAuthorizedEntity[] = "authorized_entity";
const int kTtlSeconds = 10;

class FakeGCMDriver : public gcm::FakeGCMDriver {
 public:
  FakeGCMDriver() {}
  ~FakeGCMDriver() override {}

  void SendWebPushMessage(const std::string& app_id,
                          const std::string& authorized_entity,
                          const std::string& p256dh,
                          const std::string& auth_secret,
                          const std::string& fcm_token,
                          crypto::ECPrivateKey* vapid_key,
                          gcm::WebPushMessage message,
                          gcm::WebPushCallback callback) override {
    app_id_ = app_id;
    authorized_entity_ = authorized_entity;
    p256dh_ = p256dh;
    auth_secret_ = auth_secret;
    fcm_token_ = fcm_token;
    vapid_key_ = vapid_key;
    message_ = std::move(message);
    std::move(callback).Run(result_,
                            base::make_optional<std::string>(kMessageId));
  }

  const std::string& app_id() { return app_id_; }
  const std::string& authorized_entity() { return authorized_entity_; }
  const std::string& p256dh() { return p256dh_; }
  const std::string& auth_secret() { return auth_secret_; }
  const std::string& fcm_token() { return fcm_token_; }
  crypto::ECPrivateKey* vapid_key() { return vapid_key_; }
  const gcm::WebPushMessage& message() { return message_; }

  void set_result(gcm::SendWebPushMessageResult result) { result_ = result; }

 private:
  std::string app_id_, authorized_entity_, p256dh_, auth_secret_, fcm_token_;
  crypto::ECPrivateKey* vapid_key_;
  gcm::WebPushMessage message_;
  gcm::SendWebPushMessageResult result_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMDriver);
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
                           /*last_updated_timestamp=*/base::Time::Now(),
                           /*send_tab_to_self_receiving_enabled=*/false,
                           /*sharing_info=*/base::nullopt) {}
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
  void OnMessageSent(SharingSendMessageResult* result_out,
                     base::Optional<std::string>* message_id_out,
                     SharingSendMessageResult result,
                     base::Optional<std::string> message_id) {
    *result_out = result;
    *message_id_out = std::move(message_id);
  }

 protected:
  SharingFCMSenderTest() {
    // TODO: Used fake GCMDriver
    sync_prefs_ = std::make_unique<SharingSyncPreference>(&prefs_);
    sharing_fcm_sender_ = std::make_unique<SharingFCMSender>(
        &fake_gcm_driver_, &local_device_info_provider_, sync_prefs_.get(),
        &vapid_key_manager_);
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  SharingSyncPreference::Device CreateFakeSyncDevice() {
    return SharingSyncPreference::Device(kFcmToken, kP256dh, kAuthSecret,
                                         kNoFeaturesEnabled);
  }

  std::unique_ptr<SharingSyncPreference> sync_prefs_;
  std::unique_ptr<SharingFCMSender> sharing_fcm_sender_;
  FakeGCMDriver fake_gcm_driver_;
  NiceMock<MockVapidKeyManager> vapid_key_manager_;
  FakeLocalDeviceInfoProvider local_device_info_provider_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

}  // namespace

struct SharingFCMSenderResultTestData {
  const gcm::SendWebPushMessageResult web_push_result;
  const SharingSendMessageResult expected_result;
  const bool ready_before_send_message;
} kSharingFCMSenderResultTestData[] = {
    {gcm::SendWebPushMessageResult::kSuccessful,
     SharingSendMessageResult::kSuccessful,
     /*ready_before_send_message=*/false},
    {gcm::SendWebPushMessageResult::kSuccessful,
     SharingSendMessageResult::kSuccessful,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kDeviceGone,
     SharingSendMessageResult::kDeviceNotFound,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kNetworkError,
     SharingSendMessageResult::kNetworkError,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kPayloadTooLarge,
     SharingSendMessageResult::kPayloadTooLarge,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kEncryptionFailed,
     SharingSendMessageResult::kInternalError,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kCreateJWTFailed,
     SharingSendMessageResult::kInternalError,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kServerError,
     SharingSendMessageResult::kInternalError,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kParseResponseFailed,
     SharingSendMessageResult::kInternalError,
     /*ready_before_send_message=*/true},
    {gcm::SendWebPushMessageResult::kVapidKeyInvalid,
     SharingSendMessageResult::kInternalError,
     /*ready_before_send_message=*/true}};

class SharingFCMSenderResultTest
    : public SharingFCMSenderTest,
      public testing::WithParamInterface<SharingFCMSenderResultTestData> {};

TEST_P(SharingFCMSenderResultTest, ResultTest) {
  Device target(kFcmToken, kP256dh, kAuthSecret, kNoFeaturesEnabled);

  sync_prefs_->SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, kSenderFcmToken, kSenderP256dh, kSenderAuthSecret,
      base::Time::Now()));

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(Return(vapid_key.get()));

  local_device_info_provider_.SetReady(GetParam().ready_before_send_message);

  fake_gcm_driver_.set_result(GetParam().web_push_result);

  SharingSendMessageResult result;
  base::Optional<std::string> message_id;
  SharingMessage sharing_message;
  sharing_message.mutable_ping_message();
  sharing_fcm_sender_->SendMessageToDevice(
      std::move(target), base::TimeDelta::FromSeconds(kTtlSeconds),
      std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id));

  local_device_info_provider_.SetReady(true);

  EXPECT_EQ(kSharingFCMAppID, fake_gcm_driver_.app_id());
  EXPECT_EQ(kAuthorizedEntity, fake_gcm_driver_.authorized_entity());
  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());
  EXPECT_EQ(vapid_key.get(), fake_gcm_driver_.vapid_key());

  EXPECT_EQ(kTtlSeconds, fake_gcm_driver_.message().time_to_live);
  EXPECT_EQ(gcm::WebPushMessage::Urgency::kHigh,
            fake_gcm_driver_.message().urgency);
  SharingMessage message_sent;
  message_sent.ParseFromString(fake_gcm_driver_.message().payload);
  EXPECT_EQ(kSenderGuid, message_sent.sender_guid());
  EXPECT_TRUE(message_sent.has_ping_message());
  EXPECT_TRUE(message_sent.has_sender_info());
  EXPECT_EQ(kSenderFcmToken, message_sent.sender_info().fcm_token());
  EXPECT_EQ(kSenderP256dh, message_sent.sender_info().p256dh());
  EXPECT_EQ(kSenderAuthSecret, message_sent.sender_info().auth_secret());

  EXPECT_EQ(GetParam().expected_result, result);
  EXPECT_EQ(kMessageId, message_id);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SharingFCMSenderResultTest,
    testing::ValuesIn(kSharingFCMSenderResultTestData));
