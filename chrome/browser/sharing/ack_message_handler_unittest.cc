// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/ack_message_handler.h"

#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using SharingMessage = chrome_browser_sharing::SharingMessage;
using namespace testing;

namespace {

const char kTestMessageId[] = "test_message_id";

class TestObserver : public AckMessageHandler::AckMessageObserver {
 public:
  void OnAckReceived(const std::string& message_id) override {
    received_message_id_ = message_id;
  }

  const std::string& received_message_id() { return received_message_id_; }

 private:
  std::string received_message_id_;
};

class AckMessageHandlerTest : public Test {
 protected:
  AckMessageHandlerTest() { ack_message_handler_.AddObserver(&test_observer_); }

  AckMessageHandler ack_message_handler_;
  TestObserver test_observer_;
};

}  // namespace

TEST_F(AckMessageHandlerTest, OnMessage) {
  SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);

  ack_message_handler_.OnMessage(sharing_message);

  EXPECT_EQ(kTestMessageId, test_observer_.received_message_id());
}
