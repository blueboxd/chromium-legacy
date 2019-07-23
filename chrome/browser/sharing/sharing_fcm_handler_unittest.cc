// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_handler.h"

#include <memory>

#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using SharingMessage = chrome_browser_sharing::SharingMessage;
using namespace testing;

namespace {

const char kTestAppId[] = "test_app_id";
const char kTestMessageId[] = "0:1563805165426489%0bb84dcff9fd7ecd";
const char kTestMessageIdSecondaryUser[] =
    "0:1563805165426489%20#0bb84dcff9fd7ecd";
const char kOriginalMessageId[] = "test_original_message_id";
const char kSenderGuid[] = "test_sender_guid";

class MockSharingMessageHandler : public SharingMessageHandler {
 public:
  MockSharingMessageHandler() = default;
  ~MockSharingMessageHandler() override = default;

  // SharingMessageHandler implementation:
  MOCK_METHOD1(OnMessage, void(const SharingMessage& message));
};

class MockSharingFCMSender : public SharingFCMSender {
 public:
  MockSharingFCMSender()
      : SharingFCMSender(nullptr, nullptr, nullptr, nullptr) {}
  ~MockSharingFCMSender() override {}

  MOCK_METHOD4(SendMessageToDevice,
               void(const std::string& device_guid,
                    base::TimeDelta time_to_live,
                    chrome_browser_sharing::SharingMessage message,
                    SendMessageCallback callback));
};

class SharingFCMHandlerTest : public Test {
 protected:
  SharingFCMHandlerTest() {
    sharing_fcm_handler_.reset(
        new SharingFCMHandler(&fake_gcm_driver_, &mock_sharing_fcm_sender_));
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

  NiceMock<MockSharingMessageHandler> mock_sharing_message_handler_;
  NiceMock<MockSharingFCMSender> mock_sharing_fcm_sender_;

  gcm::FakeGCMDriver fake_gcm_driver_;
  std::unique_ptr<SharingFCMHandler> sharing_fcm_handler_;
};

}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
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
              OnMessage(ProtoEquals(sharing_message)));
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToDevice(_, _, _, _))
      .Times(0);
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kAckMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

// Generic test for handling of SharingMessage payload other than AckMessage.
TEST_F(SharingFCMHandlerTest, PingMessageHandler) {
  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageId, sharing_message);

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  // Tests OnMessage flow in SharingFCMHandler when no handler is registered.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_)).Times(0);
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToDevice(_, _, _, _))
      .Times(0);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  EXPECT_CALL(mock_sharing_message_handler_,
              OnMessage(ProtoEquals(sharing_message)));
  EXPECT_CALL(mock_sharing_fcm_sender_,
              SendMessageToDevice(Eq(kSenderGuid), Eq(kAckTimeToLive),
                                  ProtoEquals(sharing_ack_message), _));
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kPingMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);

  // Tests OnMessage flow in SharingFCMHandler after registered handler is
  // removed.
  EXPECT_CALL(mock_sharing_message_handler_, OnMessage(_)).Times(0);
  EXPECT_CALL(mock_sharing_fcm_sender_, SendMessageToDevice(_, _, _, _))
      .Times(0);
  sharing_fcm_handler_->RemoveSharingHandler(SharingMessage::kPingMessage);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}

// Test for handling of SharingMessage payload other than AckMessage for
// secondary users in Android.
TEST_F(SharingFCMHandlerTest, PingMessageHandlerSecondaryUser) {
  SharingMessage sharing_message;
  sharing_message.set_sender_guid(kSenderGuid);
  sharing_message.mutable_ping_message();
  gcm::IncomingMessage incoming_message =
      CreateGCMIncomingMessage(kTestMessageIdSecondaryUser, sharing_message);

  SharingMessage sharing_ack_message;
  sharing_ack_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  // Tests OnMessage flow in SharingFCMHandler after handler is added.
  EXPECT_CALL(mock_sharing_message_handler_,
              OnMessage(ProtoEquals(sharing_message)));
  EXPECT_CALL(mock_sharing_fcm_sender_,
              SendMessageToDevice(Eq(kSenderGuid), Eq(kAckTimeToLive),
                                  ProtoEquals(sharing_ack_message), _));
  sharing_fcm_handler_->AddSharingHandler(SharingMessage::kPingMessage,
                                          &mock_sharing_message_handler_);
  sharing_fcm_handler_->OnMessage(kTestAppId, incoming_message);
}
