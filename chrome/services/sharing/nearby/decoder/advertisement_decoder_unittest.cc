// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "chrome/services/sharing/nearby/decoder/advertisement_decoder.h"

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
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

TEST(AdvertisementDecoderTest, CreateNewInstanceFromEndpointInfo) {
  std::unique_ptr<sharing::Advertisement> original =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::AdvertisementDecoder::FromEndpointInfo(
          original->ToEndpointInfo());
  ExpectEquals(*advertisement, *original);
}

TEST(AdvertisementDecoderTest, CreateNewInstanceFromStringWithExtraLength) {
  std::unique_ptr<sharing::Advertisement> original =
      sharing::Advertisement::NewInstance(
          kSalt, kEncryptedMetadataKey, base::StrCat({kDeviceName, "123456"}));
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::AdvertisementDecoder::FromEndpointInfo(
          original->ToEndpointInfo());
  ExpectEquals(*advertisement, *original);
}

TEST(AdvertisementDecoderTest,
     SerializeContactsOnlyAdvertisementWithoutDeviceName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          /* device_name= */ base::nullopt);
  ExpectEquals(*sharing::AdvertisementDecoder::FromEndpointInfo(
                   advertisement->ToEndpointInfo()),
               *advertisement);
}

TEST(AdvertisementDecoderTest,
     SerializeVisibleToEveryoneAdvertisementWithoutDeviceName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          /* device_name= */ std::string());
  EXPECT_FALSE(sharing::AdvertisementDecoder::FromEndpointInfo(
      advertisement->ToEndpointInfo()));
}

TEST(AdvertisementDecoderTest, V1ContactsOnlyAdvertisementDecoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      16, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0,  0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*sharing::AdvertisementDecoder::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementDecoderTest, V1VisibleToEveryoneAdvertisementDecoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      0, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*sharing::AdvertisementDecoder::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementDecoderTest, V1ContactsOnlyAdvertisementEncoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          /* device_name= */ base::nullopt);
  std::vector<uint8_t> v1EndpointInfo = {16, 0, 0, 0, 0, 0, 0, 0, 0,
                                         0,  0, 0, 0, 0, 0, 0, 0};
  ExpectEquals(*sharing::AdvertisementDecoder::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementDecoderTest, V1VisibleToEveryoneAdvertisementEncoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      0, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*sharing::AdvertisementDecoder::FromEndpointInfo(v1EndpointInfo),
               *advertisement);
}

TEST(AdvertisementDecoderTest, InvalidDeviceNameEncoding) {
  std::vector<uint8_t> v1EndpointInfo = {
      0, 0, 0, 0,  0,   0,  0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 226, 40, 161, 105, 99, 101, 78, 97, 109, 101,
  };
  EXPECT_FALSE(sharing::AdvertisementDecoder::FromEndpointInfo(v1EndpointInfo));
}

}  // namespace sharing
