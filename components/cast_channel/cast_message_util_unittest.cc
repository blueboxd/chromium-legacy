// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_util.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using base::test::IsJson;
using base::test::ParseJsonDeprecated;

namespace cast_channel {

TEST(CastMessageUtilTest, IsCastInternalNamespace) {
  EXPECT_TRUE(IsCastInternalNamespace("urn:x-cast:com.google.cast.receiver"));
  EXPECT_FALSE(IsCastInternalNamespace("urn:x-cast:com.google.youtube"));
  EXPECT_FALSE(IsCastInternalNamespace("urn:x-cast:com.foo"));
  EXPECT_FALSE(IsCastInternalNamespace("foo"));
  EXPECT_FALSE(IsCastInternalNamespace(""));
}

TEST(CastMessageUtilTest, CastMessageType) {
  for (int i = 0; i < static_cast<int>(CastMessageType::kOther); ++i) {
    CastMessageType type = static_cast<CastMessageType>(i);
    EXPECT_EQ(type, CastMessageTypeFromString(ToString(type)));
  }
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseOk) {
  std::string payload = R"(
    {
      "type": "RECEIVER_STATUS",
      "requestId": 123,
      "status": {}
    }
  )";

  LaunchSessionResponse response =
      GetLaunchSessionResponse(*ParseJsonDeprecated(payload));
  EXPECT_EQ(LaunchSessionResponse::Result::kOk, response.result);
  EXPECT_TRUE(response.receiver_status);
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseError) {
  std::string payload = R"(
    {
      "type": "LAUNCH_ERROR",
      "requestId": 123
    }
  )";

  LaunchSessionResponse response =
      GetLaunchSessionResponse(*ParseJsonDeprecated(payload));
  EXPECT_EQ(LaunchSessionResponse::Result::kError, response.result);
  EXPECT_FALSE(response.receiver_status);
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseUnknown) {
  // Unrelated type.
  std::string payload = R"(
    {
      "type": "APPLICATION_BROADCAST",
      "requestId": 123,
      "status": {}
    }
  )";

  LaunchSessionResponse response =
      GetLaunchSessionResponse(*ParseJsonDeprecated(payload));
  EXPECT_EQ(LaunchSessionResponse::Result::kUnknown, response.result);
  EXPECT_FALSE(response.receiver_status);
}

TEST(CastMessageUtilTest, CreateStopRequest) {
  std::string expected_message = R"(
    {
      "type": "STOP",
      "requestId": 123,
      "sessionId": "sessionId"
    }
  )";

  CastMessage message = CreateStopRequest("sourceId", 123, "sessionId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, CreateReceiverStatusRequest) {
  std::string expected_message = R"(
    {
       "type": "GET_STATUS",
       "requestId": 123
    }
  )";

  CastMessage message = CreateReceiverStatusRequest("sourceId", 123);
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, CreateMediaRequest) {
  std::string body = R"({
       "type": "STOP_MEDIA",
    })";
  std::string expected_message = R"({
       "type": "STOP",
       "requestId": 123,
    })";

  CastMessage message = CreateMediaRequest(*ParseJsonDeprecated(body), 123,
                                           "theSourceId", "theDestinationId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_EQ(kMediaNamespace, message.namespace_());
  EXPECT_EQ("theSourceId", message.source_id());
  EXPECT_EQ("theDestinationId", message.destination_id());
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, CreateVolumeRequest) {
  std::string body = R"({
       "type": "SET_VOLUME",
       "sessionId": "theSessionId",
    })";
  std::string expected_message = R"({
       "type": "SET_VOLUME",
       "requestId": 123,
    })";

  CastMessage message =
      CreateSetVolumeRequest(*ParseJsonDeprecated(body), 123, "theSourceId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_EQ(kReceiverNamespace, message.namespace_());
  EXPECT_EQ("theSourceId", message.source_id());
  EXPECT_EQ(kPlatformReceiverId, message.destination_id());
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

}  // namespace cast_channel
