// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/ack_message_handler.h"

#include "base/bind_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestObserver {
 public:
  void OnAckReceived(
      chrome_browser_sharing::MessageType message_type,
      std::string message_id,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
    received_message_type_ = message_type;
    received_message_id_ = std::move(message_id);
    received_response_ = std::move(response);
  }

  chrome_browser_sharing::MessageType received_message_type() const {
    return received_message_type_;
  }

  const std::string& received_message_id() const {
    return received_message_id_;
  }

  const chrome_browser_sharing::ResponseMessage* received_response() const {
    return received_response_.get();
  }

 private:
  chrome_browser_sharing::MessageType received_message_type_;
  std::string received_message_id_;
  std::unique_ptr<chrome_browser_sharing::ResponseMessage> received_response_;
};

class AckMessageHandlerTest : public testing::Test {
 protected:
  AckMessageHandlerTest()
      : ack_message_handler_(
            base::BindRepeating(&TestObserver::OnAckReceived,
                                base::Unretained(&test_observer_))) {}

  TestObserver test_observer_;
  AckMessageHandler ack_message_handler_;
};

bool ProtoEquals(const google::protobuf::MessageLite& expected,
                 const google::protobuf::MessageLite& actual) {
  std::string expected_serialized, actual_serialized;
  expected.SerializeToString(&expected_serialized);
  actual.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

TEST_F(AckMessageHandlerTest, OnMessageNoResponse) {
  constexpr char kTestMessageId[] = "test_message_id";

  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::CLICK_TO_CALL_MESSAGE);

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);

  ack_message_handler_.OnMessage(std::move(sharing_message),
                                 done_callback.Get());

  EXPECT_EQ(kTestMessageId, test_observer_.received_message_id());
  EXPECT_EQ(chrome_browser_sharing::CLICK_TO_CALL_MESSAGE,
            test_observer_.received_message_type());
  EXPECT_FALSE(test_observer_.received_response());
}

TEST_F(AckMessageHandlerTest, OnMessageWithResponse) {
  constexpr char kTestMessageId[] = "test_message_id";

  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::CLICK_TO_CALL_MESSAGE);
  sharing_message.mutable_ack_message()->mutable_response_message();

  chrome_browser_sharing::ResponseMessage response_message_copy =
      sharing_message.ack_message().response_message();

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);

  ack_message_handler_.OnMessage(std::move(sharing_message),
                                 done_callback.Get());

  EXPECT_EQ(kTestMessageId, test_observer_.received_message_id());
  EXPECT_EQ(chrome_browser_sharing::CLICK_TO_CALL_MESSAGE,
            test_observer_.received_message_type());
  ASSERT_TRUE(test_observer_.received_response());
  EXPECT_TRUE(
      ProtoEquals(response_message_copy, *test_observer_.received_response()));
}
