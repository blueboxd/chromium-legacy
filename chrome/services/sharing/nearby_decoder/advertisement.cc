// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/services/sharing/nearby_decoder/advertisement.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"

namespace {

// The bit mask for parsing and writing Version.
constexpr uint8_t kVersionBitmask = 0b111;

// The bit mask for parsing and writing Visibility.
constexpr uint8_t kVisibilityBitmask = 0b1;

const uint8_t kMinimumSize =
    /* Version(3 bits)|Visibility(1 bit)|Reserved(4 bits)= */ 1 +
    sharing::Advertisement::kSaltSize +
    sharing::Advertisement::kMetadataEncryptionKeyHashByteSize;

int ParseVersion(uint8_t b) {
  return (b >> 5) & kVersionBitmask;
}

uint8_t ConvertVersion(int version) {
  return static_cast<uint8_t>((version & kVersionBitmask) << 5);
}

bool ParseHasDeviceName(uint8_t b) {
  return ((b >> 4) & kVisibilityBitmask) == 0;
}

uint8_t ConvertHasDeviceName(bool hasDeviceName) {
  return static_cast<uint8_t>((hasDeviceName ? 0 : 1) << 4);
}

}  // namespace

namespace sharing {

// static
constexpr uint8_t Advertisement::kSaltSize = 2;
constexpr uint8_t Advertisement::kMetadataEncryptionKeyHashByteSize = 14;

// static
std::unique_ptr<Advertisement> Advertisement::NewInstance(
    std::vector<uint8_t> salt,
    std::vector<uint8_t> encrypted_metadata_key,
    base::Optional<std::string> device_name) {
  if (salt.size() != sharing::Advertisement::kSaltSize) {
    LOG(ERROR) << "Failed to create advertisement because the salt did "
                  "not match the expected length "
               << salt.size();
    return nullptr;
  }

  if (encrypted_metadata_key.size() !=
      sharing::Advertisement::kMetadataEncryptionKeyHashByteSize) {
    LOG(ERROR) << "Failed to create advertisement because the encrypted "
                  "metadata key did "
                  "not match the expected length "
               << encrypted_metadata_key.size();
    return nullptr;
  }

  if (device_name && device_name->size() > UINT8_MAX) {
    LOG(ERROR) << "Failed to create advertisement because device name "
                  "was over UINT8_MAX: "
               << device_name->size();
    return nullptr;
  }

  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new sharing::Advertisement(
      /* version= */ 0, std::move(salt), std::move(encrypted_metadata_key),
      std::move(device_name)));
}

// static
std::unique_ptr<Advertisement> Advertisement::FromEndpointInfo(
    base::span<const uint8_t> endpoint_info) {
  if (endpoint_info.size() < kMinimumSize) {
    LOG(ERROR) << "Failed to parse advertisement because it was too short.";
    return nullptr;
  }

  auto iter = endpoint_info.begin();
  uint8_t first_byte = *iter++;

  int version = ParseVersion(first_byte);
  if (version != 0) {
    LOG(ERROR) << "Failed to parse advertisement because we failed to parse "
                  "the version number";
    return nullptr;
  }

  bool has_device_name = ParseHasDeviceName(first_byte);

  std::vector<uint8_t> salt(iter, iter + kSaltSize);
  iter += kSaltSize;

  std::vector<uint8_t> encrypted_metadata_key(
      iter, iter + kMetadataEncryptionKeyHashByteSize);
  iter += kMetadataEncryptionKeyHashByteSize;

  int device_name_length = 0;
  if (iter != endpoint_info.end())
    device_name_length = *iter++ & 0xff;

  if (endpoint_info.end() - iter < device_name_length ||
      (device_name_length == 0 && has_device_name)) {
    LOG(ERROR) << "Failed to parse advertisement because the device name did "
                  "not match the expected length "
               << device_name_length;
    return nullptr;
  }

  base::Optional<std::string> optional_device_name;
  if (device_name_length > 0) {
    optional_device_name = std::string(iter, iter + device_name_length);
    iter += device_name_length;

    if (!base::IsStringUTF8(*optional_device_name)) {
      LOG(ERROR) << "Failed to parse advertisement because the device name was "
                    "corrupted";
      return nullptr;
    }
  }

  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new Advertisement(version, std::move(salt),
                                            std::move(encrypted_metadata_key),
                                            std::move(optional_device_name)));
}

Advertisement::~Advertisement() = default;
Advertisement::Advertisement(Advertisement&& other) = default;

std::vector<uint8_t> Advertisement::ToEndpointInfo() {
  int size = kMinimumSize + (device_name_ ? 1 : 0) +
             (device_name_ ? device_name_->size() : 0);

  std::vector<uint8_t> endpoint_info;
  endpoint_info.reserve(size);
  endpoint_info.push_back(
      static_cast<uint8_t>(ConvertVersion(version_) |
                           ConvertHasDeviceName(device_name_.has_value())));
  endpoint_info.insert(endpoint_info.end(), salt_.begin(), salt_.end());
  endpoint_info.insert(endpoint_info.end(), encrypted_metadata_key_.begin(),
                       encrypted_metadata_key_.end());

  if (device_name_) {
    endpoint_info.push_back(static_cast<uint8_t>(device_name_->size() & 0xff));
    endpoint_info.insert(endpoint_info.end(), device_name_->begin(),
                         device_name_->end());
  }
  return endpoint_info;
}

// private
Advertisement::Advertisement(int version,
                             std::vector<uint8_t> salt,
                             std::vector<uint8_t> encrypted_metadata_key,
                             base::Optional<std::string> device_name)
    : version_(version),
      salt_(std::move(salt)),
      encrypted_metadata_key_(std::move(encrypted_metadata_key)),
      device_name_(std::move(device_name)) {}

}  // namespace sharing
