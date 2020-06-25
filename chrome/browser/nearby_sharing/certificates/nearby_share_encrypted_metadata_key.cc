// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    std::vector<uint8_t> encrypted_key,
    std::vector<uint8_t> salt)
    : encrypted_key_(std::move(encrypted_key)), salt_(std::move(salt)) {
  DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKey, encrypted_key_.size());
  DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKeySalt, salt_.size());
}

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    NearbyShareEncryptedMetadataKey&&) = default;

NearbyShareEncryptedMetadataKey::~NearbyShareEncryptedMetadataKey() = default;
