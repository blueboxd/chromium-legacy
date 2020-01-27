// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/skottie_transfer_cache_entry.h"
#include "cc/paint/skottie_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

// A skottie animation with solid green color for the first 2.5 seconds and then
// a solid blue color for the next 2.5 seconds.
constexpr char kData[] =
    "{"
    "  \"v\" : \"4.12.0\","
    "  \"fr\": 30,"
    "  \"w\" : 400,"
    "  \"h\" : 200,"
    "  \"ip\": 0,"
    "  \"op\": 150,"
    "  \"assets\": [],"

    "  \"layers\": ["
    "    {"
    "      \"ty\": 1,"
    "      \"sw\": 400,"
    "      \"sh\": 200,"
    "      \"sc\": \"#00ff00\","
    "      \"ip\": 0,"
    "      \"op\": 75"
    "    },"
    "    {"
    "      \"ty\": 1,"
    "      \"sw\": 400,"
    "      \"sh\": 200,"
    "      \"sc\": \"#0000ff\","
    "      \"ip\": 76,"
    "      \"op\": 150"
    "    }"
    "  ]"
    "}";

}  // namespace

TEST(SkottieTransferCacheEntryTest, SerializationDeserialization) {
  std::vector<uint8_t> a_data(std::strlen(kData));
  a_data.assign(reinterpret_cast<const uint8_t*>(kData),
                reinterpret_cast<const uint8_t*>(kData) + std::strlen(kData));

  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateSerializable(std::move(a_data));

  // Serialize
  auto client_entry(std::make_unique<ClientSkottieTransferCacheEntry>(skottie));
  uint32_t size = client_entry->SerializedSize();
  std::vector<uint8_t> data(size);
  ASSERT_TRUE(client_entry->Serialize(
      base::make_span(static_cast<uint8_t*>(data.data()), size)));

  // De-serialize
  auto entry(std::make_unique<ServiceSkottieTransferCacheEntry>());
  ASSERT_TRUE(entry->Deserialize(
      nullptr, base::make_span(static_cast<uint8_t*>(data.data()), size)));

  EXPECT_EQ(entry->skottie()->id(), skottie->id());
  EXPECT_EQ(entry->skottie()->duration(), skottie->duration());
  EXPECT_EQ(entry->skottie()->size(), skottie->size());
}

}  // namespace cc
