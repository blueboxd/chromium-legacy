// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_handler.h"

#include <memory>

#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using SharingMessage = chrome_browser_sharing::SharingMessage;

namespace {

const char kTestAppId[] = "test_app_id";
const char kTestMessageId[] = "0:1563805165426489%0bb84dcff9fd7ecd";
const char kTestMessageIdSecondaryUser[] =
    "0:1563805165426489%20#0bb84dcff9fd7ecd";
const char kOriginalMessageId[] = "test_original_message_id";
const char kSenderGuid[] = "test_sender_guid";
const char kSenderName[] = "test_sender_name";
const char kFCMToken[] = "test_fcm_token";
const char kP256dh[] = "test_p256_dh";
const char kAuthSecret[] = "test_auth_secret";

class MockSharingMessageHandler : public SharingMessageHandler {
 public:
  MockSharingMessageHandler() = default;
  ~MockSharingMessageHandler() override = default;

  // SharingMessageHandler implementation:
  MOCK_METHOD2(OnMessage,
               void(SharingMessage message,
                    SharingMessageHandler::DoneCallback done_callback));
};

class MockSharingFCMSender : public SharingFCMSender {
 public:
  MockSharingFCMSender()
      : SharingFCMSender(/*gcm_driver=*/nullptr,
                         /*sync_preference=*/nullptr,
                         /*vapid_key_manager=*/nullptr) {}
  ~MockSharingFCMSender() override {}

  MOCK_METHOD5(SendMessageToDevice,
               void(syncer::DeviceInfo::SharingInfo target,
                    base::TimeDelta time_to_live,
                    SharingMessage message,
                    std::unique_ptr<syncer::DeviceInfo> sender_device_info,
                    SendMessageCallback callback));
};

class SharingFCMHandlerTest : public testing::Test {
 protected:
  SharingFCMHandlerTest() {
    sync_prefs_ = std::make_unique<SharingSyncPreference>(
        &prefs_, &fake_device_info_sync_service_);
    sharing_fcm_handler_ = std::make_unique<SharingFCMHandler>(
        &fake_gcm_driver_, &mock_sharing_fcm_sender_, sync_prefs_.get());
    fake_device_info_ = std::make_unique<syncer::DeviceInfo>(
        kSenderGuid, kSenderName, "chrome_version", "user_agent",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
        base::SysInfo::HardwareInfo(),
        /*last_updated_timestamp=*/base::Time::Now(),
        /*send_tab_to_self_receiving_enabled=*/false,
        syncer::DeviceInfo::SharingInfo(
            kFCMToken, kP256dh, kAuthSecret,
            std::set<sync_pb::SharingSpecificFields::EnabledFeatures>()));
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  // Creates a gcm::IncomingMessage with SharingMessage and defaults.
  gcm::IncomingMessage CreateGCMIncomingMessage(
      const std::string& message_id,
      const SharingMessage& sharing_message) {
    gcm::IncomingMessage incoming_message;
    incoming_message.message_id = message_id;
    sharing_message.SerializeToString(&incoming_message.raw_data);
    return incoming_message;
  }
  testing::NiceMock<MockSharingMessageHandler> mock_sharing_message_handler_;
  testing::NiceMock<MockSharingFCMSender> mock_sharing_fcm_sender_;

  gcm::FakeGCMDriver fake_gcm_driver_;
  std::unique_ptr<SharingFCMHandler> sharing_fcm_handler_;
  std::unique_ptr<SharingSyncPreference> sync_prefs_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;

  std::unique_ptr<syncer::DeviceInfo> fake_device_info_;
};

}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

MATCHER(DeviceMatcher, "") {
  return arg.fcm_token == kFCMToken && arg.p256dh == kP256dh &&
         arg.auth_secret == kAuthSecret;
}

// Tests handling of SharingMessage with AckMessage payload. This is different
// from other payloads since we need to ensure AckMessage is not sent for
// SharingMessage with AckMessage payload.
TEST_F(SharingFCMHandlerTest, AckMessageHandler) {
  SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kOriginalMessageId);
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  EXPECT_CALL(mock_sharing_message_handler_,
              OnMessage(ProtoEquals(sharing_message), _));
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToDevice(_, _, _, _, _))
      .Times(0);
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kAckMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

// Generic test for handling of SharingMessage payload other than AckMessage.
TEST_F(SharingFCMHandlerTest, PingMessageHandler) {
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_.get());

  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_ack_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::PING_MESSAGE);

  // Tests OnMessage flow in SharingFCMHandler when no handler is registered.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _)).Times(0);
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToDevice(_, _, _, _, _))
      .Times(0);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback).Run(/*response=*/nullptr);
          }));
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(mock_sharing_fcm_sender_,
              SendMessageToDevice(DeviceMatcher(), testing::Eq(kAckTimeToLive),
                                  ProtoEquals(sharing_ack_message),
                                  testing::Eq(nullptr), _));
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kPingMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after registered handler is
  // removed.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _)).Times(0);
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToDevice(_, _, _, _, _))
      .Times(0);
  sharing_fcm_handler_->RemoveSharingHandler(SharingMessage::kPingMessage);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

TEST_F(SharingFCMHandlerTest, PingMessageHandlerWithResponse) {
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_.get());

  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_ack_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::PING_MESSAGE);
  sharing_ack_message.mutable_ack_message()->mutable_response_message();

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke([](const SharingMessage& message,
                                        SharingMessageHandler::DoneCallback
                                            done_callback) {
        std::move(done_callback)
            .Run(std::make_unique<chrome_browser_sharing::ResponseMessage>());
      }));
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_, _));
  EXPECT_CALL(mock_sharing_fcm_sender_,
              SendMessageToDevice(DeviceMatcher(), testing::Eq(kAckTimeToLive),
                                  ProtoEquals(sharing_ack_message),
                                  testing::Eq(nullptr), _));
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kPingMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

// Test for handling of SharingMessage payload other than AckMessage for
// secondary users in Android.
TEST_F(SharingFCMHandlerTest, PingMessageHandlerSecondaryUser) {
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_.get());

  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageIdSecondaryUser, sharing_message);

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_ack_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::PING_MESSAGE);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback).Run(/*response=*/nullptr);
          }));
  EXPECT_CALL(mock_sharing_fcm_sender_,
              SendMessageToDevice(DeviceMatcher(), testing::Eq(kAckTimeToLive),
                                  ProtoEquals(sharing_ack_message),
                                  testing::Eq(nullptr), _));
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kPingMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

// Test for handling of SharingMessage payload with RecipientInfo other than
// AckMessage.
TEST_F(SharingFCMHandlerTest, PingMessageHandlerWithRecipientInfo) {
  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  chrome_browser_sharing::RecipientInfo* sender_info =
      sharing_message.mutable_sender_info();
  sender_info->set_fcm_token(kFCMToken);
  sender_info->set_p256dh(kP256dh);
  sender_info->set_auth_secret(kAuthSecret);
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_ack_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::PING_MESSAGE);

  ON_CALL(mock_sharing_message_handler_,
          OnMessage(ProtoEquals(sharing_message), _))
      .WillByDefault(testing::Invoke(
          [](const SharingMessage& message,
             SharingMessageHandler::DoneCallback done_callback) {
            std::move(done_callback).Run(/*response=*/nullptr);
          }));
  EXPECT_CALL(mock_sharing_fcm_sender_,
              SendMessageToDevice(DeviceMatcher(), testing::Eq(kAckTimeToLive),
                                  ProtoEquals(sharing_ack_message),
                                  testing::Eq(nullptr), _));
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kPingMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}
