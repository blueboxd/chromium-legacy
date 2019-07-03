// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/passphrase_enums.h"

#include "base/logging.h"

namespace syncer {

bool IsExplicitPassphrase(PassphraseType type) {
  return type == PassphraseType::CUSTOM_PASSPHRASE ||
         type == PassphraseType::FROZEN_IMPLICIT_PASSPHRASE;
}

sync_pb::NigoriSpecifics::PassphraseType ProtoPassphraseInt32ToProtoEnum(
    ::google::protobuf::int32 type) {
  return sync_pb::NigoriSpecifics::PassphraseType_IsValid(type)
             ? static_cast<sync_pb::NigoriSpecifics::PassphraseType>(type)
             : sync_pb::NigoriSpecifics::UNKNOWN;
}

base::Optional<PassphraseType> ProtoPassphraseInt32ToEnum(
    ::google::protobuf::int32 type) {
  switch (ProtoPassphraseInt32ToProtoEnum(type)) {
    case sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE:
      return PassphraseType::IMPLICIT_PASSPHRASE;
    case sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE:
      return PassphraseType::KEYSTORE_PASSPHRASE;
    case sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE:
      return PassphraseType::CUSTOM_PASSPHRASE;
    case sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      return PassphraseType::FROZEN_IMPLICIT_PASSPHRASE;
    case sync_pb::NigoriSpecifics::UNKNOWN:
      // This must be an unknown value coming from future versions or a field
      // actually being populated with UNKNOWN (which is a protocol violation).
      break;
  }

  return base::nullopt;
}

sync_pb::NigoriSpecifics::PassphraseType EnumPassphraseTypeToProto(
    PassphraseType type) {
  switch (type) {
    case PassphraseType::IMPLICIT_PASSPHRASE:
      return sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE;
    case PassphraseType::KEYSTORE_PASSPHRASE:
      return sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
    case PassphraseType::CUSTOM_PASSPHRASE:
      return sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE;
    case PassphraseType::FROZEN_IMPLICIT_PASSPHRASE:
      return sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE;
    case PassphraseType::PASSPHRASE_TYPE_SIZE:
      break;
  }

  NOTREACHED();
  return sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE;
}

KeyDerivationMethod ProtoKeyDerivationMethodToEnum(
    ::google::protobuf::int32 method) {
  DCHECK_GE(method, 0);

  switch (method) {
    case sync_pb::NigoriSpecifics::UNSPECIFIED:
      // This is the default value; it comes from an old client (<M70) which
      // does not know about this field. These old clients all use PBKDF2.
      return KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003;
    case sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003;
    case sync_pb::NigoriSpecifics::SCRYPT_8192_8_11:
      return KeyDerivationMethod::SCRYPT_8192_8_11;
  }

  // We do not know about this value. It is likely a method added in a newer
  // version of Chrome.
  return KeyDerivationMethod::UNSUPPORTED;
}

sync_pb::NigoriSpecifics::KeyDerivationMethod EnumKeyDerivationMethodToProto(
    KeyDerivationMethod method) {
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return sync_pb::NigoriSpecifics::SCRYPT_8192_8_11;
    case KeyDerivationMethod::UNSUPPORTED:
      // This value does not have a counterpart in the protocol proto enum,
      // because it is just a client side abstraction.
      break;
  }

  NOTREACHED();
  return sync_pb::NigoriSpecifics::UNSPECIFIED;
}

}  // namespace syncer
