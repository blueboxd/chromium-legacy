// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_DECODER_ADVERTISEMENT_H_
#define CHROME_SERVICES_SHARING_NEARBY_DECODER_ADVERTISEMENT_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"

namespace sharing {

// An advertisement in the form of
// [VERSION|VISIBILITY][SALT][ACCOUNT_IDENTIFIER][LEN][DEVICE_NAME].
// A device name indicates the advertisement is visible to everyone;
// a missing device name indicates the advertisement is contacts-only.
class Advertisement {
 public:
  static std::unique_ptr<Advertisement> NewInstance(
      std::vector<uint8_t> salt,
      std::vector<uint8_t> encrypted_metadata_key,
      base::Optional<std::string> device_name);

  static std::unique_ptr<Advertisement> FromEndpointInfo(
      base::span<const uint8_t> endpoint_info);

  Advertisement(Advertisement&& other);
  Advertisement(const Advertisement& other) = delete;
  Advertisement& operator=(const Advertisement& rhs) = delete;
  ~Advertisement();

  std::vector<uint8_t> ToEndpointInfo();

  int version() const { return version_; }
  const std::vector<uint8_t>& salt() const { return salt_; }
  const std::vector<uint8_t>& encrypted_metadata_key() const {
    return encrypted_metadata_key_;
  }
  const base::Optional<std::string>& device_name() const {
    return device_name_;
  }
  bool HasDeviceName() const { return device_name_.has_value(); }

  static const uint8_t kSaltSize;
  static const uint8_t kMetadataEncryptionKeyHashByteSize;

 private:
  Advertisement(int version,
                std::vector<uint8_t> salt,
                std::vector<uint8_t> encrypted_metadata_key,
                base::Optional<std::string> device_name);

  // The version of the advertisement. Different versions can have different
  // ways of parsing the endpoint id.
  int version_;

  // A randomized salt used in the hash of the account identifier.
  std::vector<uint8_t> salt_;

  // A salted hash of an account identifier that signifies who the remote device
  // is.
  std::vector<uint8_t> encrypted_metadata_key_;

  // The human readable name of the remote device.
  base::Optional<std::string> device_name_;
};

}  // namespace sharing

#endif  //  CHROME_SERVICES_SHARING_NEARBY_DECODER_ADVERTISEMENT_H_
