// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kInvalidJson[] = R"({"foo": "bar")";

constexpr char kInvalidPolicyName[] = "invalid-policy-name";

constexpr char kWallpaperJson[] = R"({
      "url": "https://example.com/device_wallpaper.jpg",
      "hash": "examplewallpaperhash"
    })";

constexpr char kWallpaperJsonInvalidValue[] = R"({
      "url": 123,
      "hash": "examplewallpaperhash"
    })";

constexpr char kWallpaperJsonUnknownProperty[] = R"({
    "url": "https://example.com/device_wallpaper.jpg",
    "hash": "examplewallpaperhash",
    "unknown-field": "random-value"
  })";

constexpr char kWallpaperUrlPropertyName[] = "url";
constexpr char kWallpaperUrlPropertyValue[] =
    "https://example.com/device_wallpaper.jpg";
constexpr char kWallpaperHashPropertyName[] = "hash";
constexpr char kWallpaperHashPropertyValue[] = "examplewallpaperhash";

}  // namespace

class DevicePolicyDecoderChromeOSTest : public testing::Test {
 public:
  DevicePolicyDecoderChromeOSTest() = default;
  ~DevicePolicyDecoderChromeOSTest() override = default;

 protected:
  std::unique_ptr<base::Value> GetWallpaperDict() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyDecoderChromeOSTest);
};

std::unique_ptr<base::Value> DevicePolicyDecoderChromeOSTest::GetWallpaperDict()
    const {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(kWallpaperUrlPropertyName,
               base::Value(kWallpaperUrlPropertyValue));
  dict->SetKey(kWallpaperHashPropertyName,
               base::Value(kWallpaperHashPropertyValue));
  return dict;
}

TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeJSONParseError) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kInvalidJson, key::kDeviceWallpaperImage, &error);
  EXPECT_FALSE(decoded_json.has_value());
  EXPECT_EQ("Invalid JSON string: Line: 1, column: 14, Syntax error.", error);
}

#if GTEST_HAS_DEATH_TEST
TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeInvalidSchema) {
  std::string error;
  EXPECT_DEATH(
      DecodeJsonStringAndNormalize(kWallpaperJson, kInvalidPolicyName, &error),
      "");
}
#endif

TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeInvalidValue) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJsonInvalidValue, key::kDeviceWallpaperImage, &error);
  EXPECT_FALSE(decoded_json.has_value());
  EXPECT_EQ(
      "Invalid policy value: The value type doesn't match the schema type. (at "
      "url)",
      error);
}

TEST_F(DevicePolicyDecoderChromeOSTest,
       DecodeJsonStringAndNormalizeUnknownProperty) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJsonUnknownProperty, key::kDeviceWallpaperImage, &error);
  EXPECT_EQ(*GetWallpaperDict(), decoded_json.value());
  EXPECT_EQ(
      "Dropped unknown properties: Unknown property: unknown-field (at "
      "toplevel)",
      error);
}

TEST_F(DevicePolicyDecoderChromeOSTest, DecodeJsonStringAndNormalizeSuccess) {
  std::string error;
  base::Optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJson, key::kDeviceWallpaperImage, &error);
  EXPECT_EQ(*GetWallpaperDict(), decoded_json.value());
  EXPECT_TRUE(error.empty());
}

}  // namespace policy
