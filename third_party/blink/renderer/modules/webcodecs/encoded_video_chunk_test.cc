// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class EncodedVideoChunkTest : public testing::Test {
 public:
  DOMArrayBuffer* StringToBuffer(std::string data) {
    return DOMArrayBuffer::Create(data.data(), data.size());
  }

  std::string BufferToString(DOMArrayBuffer* buffer) {
    return std::string(static_cast<char*>(buffer->Data()),
                       buffer->ByteLengthAsSizeT());
  }
};

TEST_F(EncodedVideoChunkTest, ConstructorAndAttributes) {
  String type = "key";
  uint64_t timestamp = 1000000;
  std::string data = "test";
  auto* encoded =
      EncodedVideoChunk::Create(type, timestamp, StringToBuffer(data));

  EXPECT_EQ(type, encoded->type());
  EXPECT_EQ(timestamp, encoded->timestamp());
  EXPECT_EQ(data, BufferToString(encoded->data()));
  EXPECT_FALSE(encoded->duration().has_value());
}

TEST_F(EncodedVideoChunkTest, ConstructorWithDuration) {
  String type = "key";
  uint64_t timestamp = 1000000;
  uint64_t duration = 16667;
  std::string data = "test";
  auto* encoded = EncodedVideoChunk::Create(type, timestamp, duration,
                                            StringToBuffer(data));

  EXPECT_EQ(type, encoded->type());
  EXPECT_EQ(timestamp, encoded->timestamp());
  EXPECT_EQ(data, BufferToString(encoded->data()));
  ASSERT_TRUE(encoded->duration().has_value());
  EXPECT_EQ(duration, encoded->duration().value());
}

}  // namespace

}  // namespace blink
