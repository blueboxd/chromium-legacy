// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "chrome/services/sharing/nearby_decoder/advertisement.h"
#include "chrome/services/sharing/nearby_decoder/nearby_decoder.h"

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

namespace {

const char kDeviceName[] = "deviceName";
// Salt for advertisement.
const std::vector<uint8_t> kSalt(Advertisement::kSaltSize, 0);
// Key for encrypting personal info metadata.
static const std::vector<uint8_t> kEncryptedMetadataKey(
    Advertisement::kMetadataEncryptionKeyHashByteSize,
    0);

void ExpectEquals(const Advertisement& self, const Advertisement& other) {
  EXPECT_EQ(self.version(), other.version());
  EXPECT_EQ(self.HasDeviceName(), other.HasDeviceName());
  EXPECT_EQ(self.device_name(), other.device_name());
  EXPECT_EQ(self.salt(), other.salt());
  EXPECT_EQ(self.encrypted_metadata_key(), other.encrypted_metadata_key());
}

}  // namespace

TEST(AdvertisementTest, CreateNewInstanceWithNullName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          /* device_name=*/base::nullopt);
  EXPECT_FALSE(advertisement->device_name());
  EXPECT_EQ(advertisement->encrypted_metadata_key(), kEncryptedMetadataKey);
  EXPECT_FALSE(advertisement->HasDeviceName());
  EXPECT_EQ(advertisement->salt(), kSalt);
}

TEST(AdvertisementTest, CreateNewInstance) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  EXPECT_EQ(advertisement->device_name(), kDeviceName);
  EXPECT_EQ(advertisement->encrypted_metadata_key(), kEncryptedMetadataKey);
  EXPECT_TRUE(advertisement->HasDeviceName());
  EXPECT_EQ(advertisement->salt(), kSalt);
}

TEST(AdvertisementTest, CreateNewInstanceFromEndpointInfo) {
  std::unique_ptr<sharing::Advertisement> original =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::FromEndpointInfo(original->ToEndpointInfo());
  ExpectEquals(*advertisement, *original);
}

TEST(AdvertisementTest, CreateNewInstanceFromStringWithExtraLength) {
  std::unique_ptr<sharing::Advertisement> original =
      sharing::Advertisement::NewInstance(
          kSalt, kEncryptedMetadataKey, base::StrCat({kDeviceName, "123456"}));
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::FromEndpointInfo(original->ToEndpointInfo());
  ExpectEquals(*advertisement, *original);
}

TEST(AdvertisementTest, CreateNewInstanceWithWrongSaltSize) {
  EXPECT_FALSE(sharing::Advertisement::NewInstance(
      /* salt= */ std::vector<uint8_t>(5, 5), kEncryptedMetadataKey,
      kDeviceName));
}

TEST(AdvertisementTest, CreateNewInstanceWithWrongAccountIdentifierSize) {
  EXPECT_FALSE(sharing::Advertisement::NewInstance(
      kSalt, /* encrypted_metadata_key= */ std::vector<uint8_t>(2, 1),
      kDeviceName));
}

TEST(AdvertisementTest, SerializeContactsOnlyAdvertisementWithoutDeviceName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          /* device_name= */ base::nullopt);
  ExpectEquals(*sharing::Advertisement::FromEndpointInfo(
                   advertisement->ToEndpointInfo()),
               *advertisement);
}

TEST(AdvertisementTest,
     SerializeVisibleToEveryoneAdvertisementWithoutDeviceName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          /* device_name= */ std::string());
  EXPECT_FALSE(sharing::Advertisement::FromEndpointInfo(
      advertisement->ToEndpointInfo()));
}

TEST(AdvertisementTest, V1ContactsOnlyAdvertisementDecoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      16, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0,  0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*sharing::Advertisement::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementTest, V1VisibleToEveryoneAdvertisementDecoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      0, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*sharing::Advertisement::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementTest, V1ContactsOnlyAdvertisementEncoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          /* device_name= */ base::nullopt);
  std::vector<uint8_t> v1EndpointInfo = {16, 0, 0, 0, 0, 0, 0, 0, 0,
                                         0,  0, 0, 0, 0, 0, 0, 0};
  ExpectEquals(*sharing::Advertisement::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementTest, V1VisibleToEveryoneAdvertisementEncoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      0, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*sharing::Advertisement::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementTest, InvalidDeviceNameEncoding) {
  std::vector<uint8_t> v1EndpointInfo = {
      0, 0, 0, 0,  0,   0,  0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 226, 40, 161, 105, 99, 101, 78, 97, 109, 101,
  };
  EXPECT_FALSE(sharing::Advertisement::FromEndpointInfo(v1EndpointInfo));
}

}  // namespace sharing
