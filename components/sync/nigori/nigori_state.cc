// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_state.h"

#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace syncer {

namespace {

std::string ComputePbkdf2KeyName(const std::string& password) {
  std::string key_name;
  Nigori::CreateByDerivation(KeyDerivationParams::CreateForPbkdf2(), password)
      ->Permute(Nigori::Password, kNigoriKeyName, &key_name);
  return key_name;
}

sync_pb::CustomPassphraseKeyDerivationParams
CustomPassphraseKeyDerivationParamsToProto(const KeyDerivationParams& params) {
  sync_pb::CustomPassphraseKeyDerivationParams output;
  output.set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(params.method()));
  if (params.method() == KeyDerivationMethod::SCRYPT_8192_8_11) {
    output.set_custom_passphrase_key_derivation_salt(params.scrypt_salt());
  }
  return output;
}

KeyDerivationParams CustomPassphraseKeyDerivationParamsFromProto(
    const sync_pb::CustomPassphraseKeyDerivationParams& proto) {
  switch (ProtoKeyDerivationMethodToEnum(
      proto.custom_passphrase_key_derivation_method())) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(
          proto.custom_passphrase_key_derivation_salt());
    case KeyDerivationMethod::UNSUPPORTED:
      break;
  }
  NOTREACHED();
  return KeyDerivationParams::CreateWithUnsupportedMethod();
}

}  // namespace

// static
NigoriState NigoriState::CreateFromProto(const sync_pb::NigoriModel& proto) {
  NigoriState state;

  state.cryptographer =
      CryptographerImpl::FromProto(proto.cryptographer_data());

  if (proto.has_pending_keys()) {
    state.pending_keys = proto.pending_keys();
  }

  state.passphrase_type = proto.passphrase_type();
  state.keystore_migration_time =
      ProtoTimeToTime(proto.keystore_migration_time());
  state.custom_passphrase_time =
      ProtoTimeToTime(proto.custom_passphrase_time());
  if (proto.has_custom_passphrase_key_derivation_params()) {
    state.custom_passphrase_key_derivation_params =
        CustomPassphraseKeyDerivationParamsFromProto(
            proto.custom_passphrase_key_derivation_params());
  }
  state.encrypt_everything = proto.encrypt_everything();
  for (int i = 0; i < proto.keystore_key_size(); ++i) {
    state.keystore_keys.push_back(proto.keystore_key(i));
  }
  return state;
}

NigoriState::NigoriState()
    : cryptographer(CryptographerImpl::CreateEmpty()),
      passphrase_type(kInitialPassphraseType),
      encrypt_everything(kInitialEncryptEverything) {}

NigoriState::NigoriState(NigoriState&& other) = default;

NigoriState::~NigoriState() = default;

NigoriState& NigoriState::operator=(NigoriState&& other) = default;

sync_pb::NigoriModel NigoriState::ToProto() const {
  sync_pb::NigoriModel proto;
  *proto.mutable_cryptographer_data() = cryptographer->ToProto();
  if (pending_keys.has_value()) {
    *proto.mutable_pending_keys() = *pending_keys;
  }
  if (!keystore_keys.empty()) {
    proto.set_current_keystore_key_name(
        ComputePbkdf2KeyName(keystore_keys.back()));
  }
  proto.set_passphrase_type(passphrase_type);
  if (!keystore_migration_time.is_null()) {
    proto.set_keystore_migration_time(TimeToProtoTime(keystore_migration_time));
  }
  if (!custom_passphrase_time.is_null()) {
    proto.set_custom_passphrase_time(TimeToProtoTime(custom_passphrase_time));
  }
  if (custom_passphrase_key_derivation_params) {
    *proto.mutable_custom_passphrase_key_derivation_params() =
        CustomPassphraseKeyDerivationParamsToProto(
            *custom_passphrase_key_derivation_params);
  }
  proto.set_encrypt_everything(encrypt_everything);
  ModelTypeSet encrypted_types = SyncEncryptionHandler::SensitiveTypes();
  if (encrypt_everything) {
    encrypted_types = EncryptableUserTypes();
  }
  for (ModelType model_type : encrypted_types) {
    proto.add_encrypted_types_specifics_field_number(
        GetSpecificsFieldNumberFromModelType(model_type));
  }
  // TODO(crbug.com/970213): we currently store keystore keys in proto only to
  // allow rollback of USS Nigori. Having keybag with all keystore keys and
  // |current_keystore_key_name| is enough to support all logic. We should
  // remove them few milestones after USS migration completed.
  for (const std::string& keystore_key : keystore_keys) {
    proto.add_keystore_key(keystore_key);
  }
  return proto;
}

}  // namespace syncer
