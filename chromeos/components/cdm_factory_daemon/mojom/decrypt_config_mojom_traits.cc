// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/mojom/decrypt_config_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

using MojomDecryptStatus = chromeos::cdm::mojom::DecryptStatus;
using NativeDecryptStatus = media::Decryptor::Status;
using MojomEncryptionScheme = chromeos::cdm::mojom::EncryptionScheme;
using NativeEncryptionScheme = media::EncryptionScheme;

// static
MojomDecryptStatus EnumTraits<MojomDecryptStatus, NativeDecryptStatus>::ToMojom(
    NativeDecryptStatus input) {
  switch (input) {
    case NativeDecryptStatus::kSuccess:
      return MojomDecryptStatus::kSuccess;
    case NativeDecryptStatus::kNoKey:
      return MojomDecryptStatus::kNoKey;
    case NativeDecryptStatus::kNeedMoreData:
      return MojomDecryptStatus::kFailure;
    case NativeDecryptStatus::kError:
      return MojomDecryptStatus::kFailure;
  }
  NOTREACHED();
  return MojomDecryptStatus::kFailure;
}

// static
bool EnumTraits<MojomDecryptStatus, NativeDecryptStatus>::FromMojom(
    MojomDecryptStatus input,
    NativeDecryptStatus* out) {
  switch (input) {
    case MojomDecryptStatus::kSuccess:
      *out = NativeDecryptStatus::kSuccess;
      return true;
    case MojomDecryptStatus::kNoKey:
      *out = NativeDecryptStatus::kNoKey;
      return true;
    case MojomDecryptStatus::kFailure:
      *out = NativeDecryptStatus::kError;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
MojomEncryptionScheme
EnumTraits<MojomEncryptionScheme, NativeEncryptionScheme>::ToMojom(
    NativeEncryptionScheme input) {
  switch (input) {
    // We should never encounter the unencrypted value.
    case NativeEncryptionScheme::kUnencrypted:
      NOTREACHED();
      return MojomEncryptionScheme::kCenc;
    case NativeEncryptionScheme::kCenc:
      return MojomEncryptionScheme::kCenc;
    case NativeEncryptionScheme::kCbcs:
      return MojomEncryptionScheme::kCbcs;
  }
  NOTREACHED();
  return MojomEncryptionScheme::kCenc;
}

// static
bool EnumTraits<MojomEncryptionScheme, NativeEncryptionScheme>::FromMojom(
    MojomEncryptionScheme input,
    NativeEncryptionScheme* out) {
  switch (input) {
    case MojomEncryptionScheme::kCenc:
      *out = NativeEncryptionScheme::kCenc;
      return true;
    case MojomEncryptionScheme::kCbcs:
      *out = NativeEncryptionScheme::kCbcs;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<chromeos::cdm::mojom::EncryptionPatternDataView,
                  media::EncryptionPattern>::
    Read(chromeos::cdm::mojom::EncryptionPatternDataView input,
         media::EncryptionPattern* output) {
  *output = media::EncryptionPattern(input.crypt_byte_block(),
                                     input.skip_byte_block());
  return true;
}

}  // namespace mojo