// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/types.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

std::string ToJSON(const base::Value& value) {
  std::string json;
  CHECK(base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json));
  // Don't use \r\n on windows.
  base::RemoveChars(json, "\r", &json);
  return json;
}

DebugStreamData MakeDebugStreamData() {
  NetworkResponseInfo fetch_info;
  fetch_info.status_code = 200;
  fetch_info.fetch_duration = base::TimeDelta::FromSeconds(4);
  fetch_info.fetch_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(200);
  fetch_info.bless_nonce = "nonce";
  fetch_info.base_request_url = GURL("https://www.google.com");

  DebugStreamData data;
  data.fetch_info = fetch_info;
  data.load_stream_status = "loaded OK";
  return data;
}
}  // namespace

TEST(DebugStreamData, CanSerialize) {
  const DebugStreamData test_data = MakeDebugStreamData();
  const auto serialized = SerializeDebugStreamData(test_data);
  base::Optional<DebugStreamData> result =
      DeserializeDebugStreamData(serialized);
  ASSERT_TRUE(result);

  EXPECT_EQ(SerializeDebugStreamData(*result), serialized);

  ASSERT_TRUE(result->fetch_info);
  EXPECT_EQ(test_data.fetch_info->status_code, result->fetch_info->status_code);
  EXPECT_EQ(test_data.fetch_info->fetch_duration,
            result->fetch_info->fetch_duration);
  EXPECT_EQ(test_data.fetch_info->fetch_time, result->fetch_info->fetch_time);
  EXPECT_EQ(test_data.fetch_info->bless_nonce, result->fetch_info->bless_nonce);
  EXPECT_EQ(test_data.fetch_info->base_request_url,
            result->fetch_info->base_request_url);
  EXPECT_EQ(test_data.load_stream_status, result->load_stream_status);
}

TEST(DebugStreamData, CanSerializeWithoutFetchInfo) {
  DebugStreamData input = MakeDebugStreamData();
  input.fetch_info = base::nullopt;

  const auto serialized = SerializeDebugStreamData(input);
  base::Optional<DebugStreamData> result =
      DeserializeDebugStreamData(serialized);
  ASSERT_TRUE(result);

  EXPECT_EQ(SerializeDebugStreamData(*result), serialized);
}

TEST(DebugStreamData, FailsDeserializationGracefully) {
  ASSERT_EQ(base::nullopt, DeserializeDebugStreamData({}));
}

TEST(PersistentMetricsData, SerializesAndDeserializes) {
  PersistentMetricsData data;
  data.accumulated_time_spent_in_feed = base::TimeDelta::FromHours(2);
  data.current_day_start = base::Time::UnixEpoch();

  const base::Value serialized_value = PersistentMetricsDataToValue(data);
  const PersistentMetricsData deserialized_value =
      PersistentMetricsDataFromValue(serialized_value);

  EXPECT_EQ(R"({
   "day_start": "11644473600000000",
   "time_spent_in_feed": "7200000000"
}
)",
            ToJSON(serialized_value));
  EXPECT_EQ(data.accumulated_time_spent_in_feed,
            deserialized_value.accumulated_time_spent_in_feed);
  EXPECT_EQ(data.current_day_start, deserialized_value.current_day_start);
}

}  // namespace feed
