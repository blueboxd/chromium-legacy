// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/base/encryptor.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/nigori/nigori_storage.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

namespace {

using sync_pb::NigoriSpecifics;

const char kNigoriNonUniqueName[] = "Nigori";

std::unique_ptr<CryptographerImpl> CreateCryptographerFromKeystoreKeys(
    const std::vector<std::string>& keystore_keys) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();

  if (keystore_keys.empty()) {
    return cryptographer;
  }

  std::string last_key_name;
  for (const std::string& key : keystore_keys) {
    last_key_name =
        cryptographer->EmplaceKey(key, KeyDerivationParams::CreateForPbkdf2());
    // TODO(crbug.com/922900): possible behavioral change. Old implementation
    // fails only if we failed to add current keystore key. Failing to add any
    // of these keys doesn't seem valid. This line seems to be a good candidate
    // for UMA, as it's not a normal situation, if we fail to add any key.
    if (last_key_name.empty()) {
      return nullptr;
    }
  }

  DCHECK(!last_key_name.empty());
  cryptographer->SelectDefaultEncryptionKey(last_key_name);

  return cryptographer;
}

// |encrypted| must not be null.
bool EncryptKeyBag(const CryptographerImpl& cryptographer,
                   sync_pb::EncryptedData* encrypted) {
  DCHECK(encrypted);
  DCHECK(cryptographer.CanEncrypt());

  sync_pb::CryptographerData proto = cryptographer.ToProto();
  DCHECK(!proto.key_bag().key().empty());

  // Encrypt the bag with the default Nigori.
  return cryptographer.Encrypt(proto.key_bag(), encrypted);
}

// TODO(crbug.com/922900): This should be revisited because the encryption key
// should be determined by keystore keys, which does not always match the
// default encryption key.
bool EncryptKeystoreDecryptorToken(
    const CryptographerImpl& cryptographer,
    sync_pb::EncryptedData* keystore_decryptor_token) {
  DCHECK(keystore_decryptor_token);

  const sync_pb::NigoriKey default_key = cryptographer.ExportDefaultKey();

  return cryptographer.EncryptString(default_key.SerializeAsString(),
                                     keystore_decryptor_token);
}

// Attempts to decrypt |keystore_decryptor_token| with |keystore_keys|. Returns
// a keybag with one key in case of success or an empty one in case of error.
NigoriKeyBag DecryptKeystoreDecryptorToken(
    const std::vector<std::string>& keystore_keys,
    const sync_pb::EncryptedData& keystore_decryptor_token) {
  if (keystore_decryptor_token.blob().empty()) {
    return NigoriKeyBag::CreateEmpty();
  }

  std::unique_ptr<Cryptographer> cryptographer =
      CreateCryptographerFromKeystoreKeys(keystore_keys);

  sync_pb::NigoriKey key;
  // This check should never fail as long as we don't receive invalid data.
  if (!cryptographer || !cryptographer->CanDecrypt(keystore_decryptor_token) ||
      !cryptographer->Decrypt(keystore_decryptor_token, &key)) {
    return NigoriKeyBag::CreateEmpty();
  }

  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  key_bag.AddKeyFromProto(key);
  return key_bag;
}

// Creates keystore Nigori specifics given |keystore_keys|.
// Returns new NigoriSpecifics if successful and base::nullopt otherwise. If
// successful the result will contain:
// 1. passphrase_type = KEYSTORE_PASSPHRASE.
// 2. encryption_keybag contains all |keystore_keys| and encrypted with the
// latest keystore key.
// 3. keystore_decryptor_token contains latest keystore key encrypted with
// itself.
// 4. keybag_is_frozen = true.
// 5. keystore_migration_time is current time.
// 6. Other fields are default.
base::Optional<NigoriSpecifics> MakeDefaultKeystoreNigori(
    const std::vector<std::string>& keystore_keys) {
  DCHECK(!keystore_keys.empty());

  std::unique_ptr<CryptographerImpl> cryptographer =
      CreateCryptographerFromKeystoreKeys(keystore_keys);
  if (!cryptographer) {
    return base::nullopt;
  }

  NigoriSpecifics specifics;
  if (!EncryptKeystoreDecryptorToken(
          *cryptographer, specifics.mutable_keystore_decryptor_token())) {
    DLOG(ERROR) << "Failed to encrypt default key as keystore_decryptor_token.";
    return base::nullopt;
  }
  if (!EncryptKeyBag(*cryptographer, specifics.mutable_encryption_keybag())) {
    DLOG(ERROR) << "Failed to encrypt keystore keys into encryption_keybag.";
    return base::nullopt;
  }
  specifics.set_passphrase_type(NigoriSpecifics::KEYSTORE_PASSPHRASE);
  // Let non-USS client know, that Nigori doesn't need migration.
  specifics.set_keybag_is_frozen(true);
  specifics.set_keystore_migration_time(TimeToProtoTime(base::Time::Now()));
  return specifics;
}

// Returns the key derivation method to be used when a user sets a new
// custom passphrase.
KeyDerivationMethod GetDefaultKeyDerivationMethodForCustomPassphrase() {
  if (base::FeatureList::IsEnabled(
          switches::kSyncUseScryptForNewCustomPassphrases) &&
      !base::FeatureList::IsEnabled(
          switches::kSyncForceDisableScryptForCustomPassphrase)) {
    return KeyDerivationMethod::SCRYPT_8192_8_11;
  }

  return KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003;
}

KeyDerivationParams CreateKeyDerivationParamsForCustomPassphrase(
    const base::RepeatingCallback<std::string()>& random_salt_generator) {
  KeyDerivationMethod method =
      GetDefaultKeyDerivationMethodForCustomPassphrase();
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(random_salt_generator.Run());
    case KeyDerivationMethod::UNSUPPORTED:
      break;
  }

  NOTREACHED();
  return KeyDerivationParams::CreateWithUnsupportedMethod();
}

KeyDerivationMethod GetKeyDerivationMethodFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  KeyDerivationMethod key_derivation_method = ProtoKeyDerivationMethodToEnum(
      specifics.custom_passphrase_key_derivation_method());
  if (key_derivation_method == KeyDerivationMethod::SCRYPT_8192_8_11 &&
      base::FeatureList::IsEnabled(
          switches::kSyncForceDisableScryptForCustomPassphrase)) {
    // Because scrypt is explicitly disabled, just behave as if it is an
    // unsupported method.
    key_derivation_method = KeyDerivationMethod::UNSUPPORTED;
  }

  return key_derivation_method;
}

std::string GetScryptSaltFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  DCHECK_EQ(specifics.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::SCRYPT_8192_8_11);
  std::string decoded_salt;
  bool result = base::Base64Decode(
      specifics.custom_passphrase_key_derivation_salt(), &decoded_salt);
  DCHECK(result);
  return decoded_salt;
}

KeyDerivationParams GetKeyDerivationParamsFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  KeyDerivationMethod method = GetKeyDerivationMethodFromSpecifics(specifics);
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(
          GetScryptSaltFromSpecifics(specifics));
    case KeyDerivationMethod::UNSUPPORTED:
      break;
  }

  return KeyDerivationParams::CreateWithUnsupportedMethod();
}

void UpdateSpecificsFromKeyDerivationParams(
    const KeyDerivationParams& params,
    sync_pb::NigoriSpecifics* specifics) {
  DCHECK_EQ(specifics->passphrase_type(),
            sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  DCHECK_NE(params.method(), KeyDerivationMethod::UNSUPPORTED);
  specifics->set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(params.method()));
  if (params.method() == KeyDerivationMethod::SCRYPT_8192_8_11) {
    // Persist the salt used for key derivation in Nigori if we're using scrypt.
    std::string encoded_salt;
    base::Base64Encode(params.scrypt_salt(), &encoded_salt);
    specifics->set_custom_passphrase_key_derivation_salt(encoded_salt);
  }
}

bool SpecificsHasValidKeyDerivationParams(const NigoriSpecifics& specifics) {
  switch (GetKeyDerivationMethodFromSpecifics(specifics)) {
    case KeyDerivationMethod::UNSUPPORTED:
      DLOG(ERROR) << "Unsupported key derivation method encountered: "
                  << specifics.custom_passphrase_key_derivation_method();
      return false;
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return true;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      if (!specifics.has_custom_passphrase_key_derivation_salt()) {
        DLOG(ERROR) << "Missed key derivation salt while key derivation "
                    << "method is SCRYPT_8192_8_11.";
        return false;
      }
      std::string temp;
      if (!base::Base64Decode(specifics.custom_passphrase_key_derivation_salt(),
                              &temp)) {
        DLOG(ERROR) << "Key derivation salt is not a valid base64 encoded "
                       "string.";
        return false;
      }
      return true;
  }
}

// Validates given |specifics| assuming it's not specifics received from the
// server during first-time sync for current user (i.e. it's not a default
// specifics).
bool IsValidNigoriSpecifics(const NigoriSpecifics& specifics) {
  if (specifics.encryption_keybag().blob().empty()) {
    DLOG(ERROR) << "Specifics contains empty encryption_keybag.";
    return false;
  }
  // TODO(mmoskvitin): Revisit this because of IMPLICIT_PASSPHRASE, which also
  // contradicts a later comment.
  if (!specifics.has_passphrase_type()) {
    DLOG(ERROR) << "Specifics has no passphrase_type.";
    return false;
  }
  switch (ProtoPassphraseInt32ToProtoEnum(specifics.passphrase_type())) {
    case NigoriSpecifics::UNKNOWN:
      DLOG(ERROR) << "Received unknown passphrase type with value: "
                  << specifics.passphrase_type();
      return false;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      // TODO(crbug.com/922900): we hope that IMPLICIT_PASSPHRASE support is not
      // needed in new implementation. In case we need to continue migration to
      // keystore we will need to support it.
      // Note: in case passphrase_type is not set, we also end up here, because
      // IMPLICIT_PASSPHRASE is a default value.
      DLOG(ERROR) << "IMPLICIT_PASSPHRASE is not supported.";
      return false;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      if (specifics.keystore_decryptor_token().blob().empty()) {
        DLOG(ERROR) << "Keystore Nigori should have filled "
                    << "keystore_decryptor_token.";
        return false;
      }
      break;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      if (!SpecificsHasValidKeyDerivationParams(specifics)) {
        return false;
      }
      FALLTHROUGH;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      if (!specifics.encrypt_everything()) {
        DLOG(ERROR) << "Nigori with explicit passphrase type should have "
                       "enabled encrypt_everything.";
        return false;
      }
      break;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return base::FeatureList::IsEnabled(
          switches::kSyncSupportTrustedVaultPassphrase);
  }
  return true;
}

bool IsValidPassphraseTransition(
    NigoriSpecifics::PassphraseType old_passphrase_type,
    NigoriSpecifics::PassphraseType new_passphrase_type) {
  // We never allow setting IMPLICIT_PASSPHRASE as current passphrase type.
  DCHECK_NE(old_passphrase_type, NigoriSpecifics::IMPLICIT_PASSPHRASE);
  // We assume that |new_passphrase_type| is valid.
  DCHECK_NE(new_passphrase_type, NigoriSpecifics::UNKNOWN);
  DCHECK_NE(new_passphrase_type, NigoriSpecifics::IMPLICIT_PASSPHRASE);

  if (old_passphrase_type == new_passphrase_type) {
    return true;
  }
  switch (old_passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      // This can happen iff we have not synced local state yet, so we accept
      // any valid passphrase type (invalid filtered before).
      return true;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      NOTREACHED();
      return false;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      return new_passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE ||
             new_passphrase_type == NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      // There is no client side code which can cause such transition, but
      // technically it's a valid one and can be implemented in the future.
      return new_passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      return false;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      // TODO(crbug.com/984940): The below should be allowed for
      // CUSTOM_PASSPHRASE and KEYSTORE_PASSPHRASE but it requires carefully
      // verifying that the client triggering the transition already had access
      // to the trusted vault passphrase (e.g. the new keybag must be a
      // superset of the old and the default key must have changed).
      NOTIMPLEMENTED();
      return false;
  }
  NOTREACHED();
  return false;
}

// Updates |*current_type| if needed. Returns true if its value was changed.
bool UpdatePassphraseType(NigoriSpecifics::PassphraseType new_type,
                          NigoriSpecifics::PassphraseType* current_type) {
  DCHECK(current_type);
  DCHECK(IsValidPassphraseTransition(*current_type, new_type));
  if (*current_type == new_type) {
    return false;
  }
  *current_type = new_type;
  return true;
}

bool IsValidEncryptedTypesTransition(bool old_encrypt_everything,
                                     const NigoriSpecifics& specifics) {
  // We don't support relaxing the encryption requirements.
  // TODO(crbug.com/922900): more logic is to be added here, once we support
  // enforced encryption for individual datatypes.
  return specifics.encrypt_everything() || !old_encrypt_everything;
}

// Updates |*current_encrypt_everything| if needed. Returns true if its value
// was changed.
bool UpdateEncryptedTypes(const NigoriSpecifics& specifics,
                          bool* current_encrypt_everything) {
  DCHECK(current_encrypt_everything);
  DCHECK(
      IsValidEncryptedTypesTransition(*current_encrypt_everything, specifics));
  // TODO(crbug.com/922900): more logic is to be added here, once we support
  // enforced encryption for individual datatypes.
  if (*current_encrypt_everything == specifics.encrypt_everything()) {
    return false;
  }
  *current_encrypt_everything = specifics.encrypt_everything();
  return true;
}

// Updates |specifics|'s individual datatypes encryption state based on
// |encrypted_types|.
void UpdateNigoriSpecificsFromEncryptedTypes(
    ModelTypeSet encrypted_types,
    sync_pb::NigoriSpecifics* specifics) {
  static_assert(39 == ModelType::NUM_ENTRIES,
                "If adding an encryptable type, update handling below.");
  specifics->set_encrypt_bookmarks(encrypted_types.Has(BOOKMARKS));
  specifics->set_encrypt_preferences(encrypted_types.Has(PREFERENCES));
  specifics->set_encrypt_autofill_profile(
      encrypted_types.Has(AUTOFILL_PROFILE));
  specifics->set_encrypt_autofill(encrypted_types.Has(AUTOFILL));
  specifics->set_encrypt_autofill_wallet_metadata(
      encrypted_types.Has(AUTOFILL_WALLET_METADATA));
  specifics->set_encrypt_themes(encrypted_types.Has(THEMES));
  specifics->set_encrypt_typed_urls(encrypted_types.Has(TYPED_URLS));
  specifics->set_encrypt_extensions(encrypted_types.Has(EXTENSIONS));
  specifics->set_encrypt_search_engines(encrypted_types.Has(SEARCH_ENGINES));
  specifics->set_encrypt_sessions(encrypted_types.Has(SESSIONS));
  specifics->set_encrypt_apps(encrypted_types.Has(APPS));
  specifics->set_encrypt_app_settings(encrypted_types.Has(APP_SETTINGS));
  specifics->set_encrypt_extension_settings(
      encrypted_types.Has(EXTENSION_SETTINGS));
  specifics->set_encrypt_dictionary(encrypted_types.Has(DICTIONARY));
  specifics->set_encrypt_favicon_images(encrypted_types.Has(FAVICON_IMAGES));
  specifics->set_encrypt_favicon_tracking(
      encrypted_types.Has(FAVICON_TRACKING));
  specifics->set_encrypt_app_list(encrypted_types.Has(APP_LIST));
  specifics->set_encrypt_arc_package(encrypted_types.Has(ARC_PACKAGE));
  specifics->set_encrypt_printers(encrypted_types.Has(PRINTERS));
  specifics->set_encrypt_reading_list(encrypted_types.Has(READING_LIST));
  specifics->set_encrypt_mountain_shares(encrypted_types.Has(MOUNTAIN_SHARES));
  specifics->set_encrypt_send_tab_to_self(
      encrypted_types.Has(SEND_TAB_TO_SELF));
  specifics->set_encrypt_web_apps(encrypted_types.Has(WEB_APPS));
}

// Packs explicit passphrase key in order to persist it. Should be aligned with
// Directory implementation (Cryptographer::GetBootstrapToken()) unless it is
// removed. Returns empty string in case of errors.
std::string PackExplicitPassphraseKey(const Encryptor& encryptor,
                                      const CryptographerImpl& cryptographer) {
  DCHECK(cryptographer.CanEncrypt());

  // Explicit passphrase key should always be default one.
  std::string serialized_key =
      cryptographer.ExportDefaultKey().SerializeAsString();

  if (serialized_key.empty()) {
    DLOG(ERROR) << "Failed to serialize explicit passphrase key.";
    return std::string();
  }

  std::string encrypted_key;
  if (!encryptor.EncryptString(serialized_key, &encrypted_key)) {
    DLOG(ERROR) << "Failed to encrypt explicit passphrase key.";
    return std::string();
  }

  std::string encoded_key;
  base::Base64Encode(encrypted_key, &encoded_key);
  return encoded_key;
}

// Unpacks explicit passphrase keys. Returns a populated sync_pb::NigoriKey if
// successful, or an empty instance (i.e. default value) if |packed_key| is
// empty or decoding/decryption errors occur.
// Should be aligned with Directory implementation (
// Cryptographer::UnpackBootstrapToken()) unless it is removed.
sync_pb::NigoriKey UnpackExplicitPassphraseKey(const Encryptor& encryptor,
                                               const std::string& packed_key) {
  if (packed_key.empty()) {
    return sync_pb::NigoriKey();
  }

  std::string decoded_key;
  if (!base::Base64Decode(packed_key, &decoded_key)) {
    DLOG(ERROR) << "Failed to decode explicit passphrase key.";
    return sync_pb::NigoriKey();
  }

  std::string decrypted_key;
  if (!encryptor.DecryptString(decoded_key, &decrypted_key)) {
    DLOG(ERROR) << "Failed to decrypt expliciti passphrase key.";
    return sync_pb::NigoriKey();
  }

  sync_pb::NigoriKey key;
  key.ParseFromString(decrypted_key);
  return key;
}

ModelTypeSet GetEncryptedTypes(bool encrypt_everything) {
  if (encrypt_everything) {
    return EncryptableUserTypes();
  }
  return SyncEncryptionHandler::SensitiveTypes();
}

}  // namespace

NigoriSyncBridgeImpl::NigoriSyncBridgeImpl(
    std::unique_ptr<NigoriLocalChangeProcessor> processor,
    std::unique_ptr<NigoriStorage> storage,
    const Encryptor* encryptor,
    const base::RepeatingCallback<std::string()>& random_salt_generator,
    const std::string& packed_explicit_passphrase_key)
    : encryptor_(encryptor),
      processor_(std::move(processor)),
      storage_(std::move(storage)),
      random_salt_generator_(random_salt_generator),
      explicit_passphrase_key_(
          UnpackExplicitPassphraseKey(*encryptor,
                                      packed_explicit_passphrase_key)) {
  DCHECK(encryptor);

  // TODO(crbug.com/922900): we currently don't verify |deserialized_data|.
  // It's quite unlikely we get a corrupted data, since it was successfully
  // deserialized and decrypted. But we may want to consider some
  // verifications, taking into account sensitivity of this data.
  base::Optional<sync_pb::NigoriLocalData> deserialized_data =
      storage_->RestoreData();
  if (!deserialized_data ||
      (deserialized_data->nigori_model().passphrase_type() ==
           NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE &&
       !base::FeatureList::IsEnabled(
           switches::kSyncSupportTrustedVaultPassphrase))) {
    // We either have no Nigori node stored locally or it was corrupted.
    processor_->ModelReadyToSync(this, NigoriMetadataBatch());
    return;
  }

  // Restore data.
  state_ =
      syncer::NigoriState::CreateFromProto(deserialized_data->nigori_model());

  // Restore metadata.
  NigoriMetadataBatch metadata_batch;
  metadata_batch.model_type_state = deserialized_data->model_type_state();
  metadata_batch.entity_metadata = deserialized_data->entity_metadata();
  processor_->ModelReadyToSync(this, std::move(metadata_batch));
}

NigoriSyncBridgeImpl::~NigoriSyncBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriSyncBridgeImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void NigoriSyncBridgeImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool NigoriSyncBridgeImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We need to expose whole bridge state through notifications, because it
  // can be different from default due to restoring from the file or
  // completeness of first sync cycle (which happens before Init() call).
  // TODO(crbug.com/922900): try to avoid double notification (second one can
  // happen during UpdateLocalState() call).
  if (state_.pending_keys.has_value()) {
    for (auto& observer : observers_) {
      observer.OnPassphraseRequired(REASON_DECRYPTION,
                                    GetKeyDerivationParamsForPendingKeys(),
                                    *state_.pending_keys);
    }
  }
  for (auto& observer : observers_) {
    observer.OnEncryptedTypesChanged(
        GetEncryptedTypes(state_.encrypt_everything),
        state_.encrypt_everything);
  }
  for (auto& observer : observers_) {
    observer.OnCryptographerStateChanged(state_.cryptographer.get(),
                                         state_.pending_keys.has_value());
  }
  if (state_.passphrase_type != NigoriSpecifics::UNKNOWN) {
    // if |passphrase_type| is unknown, it is not yet initialized and we
    // shouldn't expose it.
    PassphraseType enum_passphrase_type =
        *ProtoPassphraseInt32ToEnum(state_.passphrase_type);
    for (auto& observer : observers_) {
      observer.OnPassphraseTypeChanged(enum_passphrase_type,
                                       GetExplicitPassphraseTime());
    }
    UMA_HISTOGRAM_ENUMERATION("Sync.PassphraseType", enum_passphrase_type);
  }
  UMA_HISTOGRAM_BOOLEAN("Sync.CryptographerReady",
                        state_.cryptographer->CanEncrypt());
  UMA_HISTOGRAM_BOOLEAN("Sync.CryptographerPendingKeys",
                        state_.pending_keys.has_value());
  if (state_.pending_keys.has_value() &&
      state_.passphrase_type == NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    // If this is happening, it means the keystore decryptor is either
    // undecryptable with the available keystore keys or does not match the
    // nigori keybag's encryption key. Otherwise we're simply missing the
    // keystore key.
    UMA_HISTOGRAM_BOOLEAN("Sync.KeystoreDecryptionFailed",
                          !state_.keystore_keys.empty());
  }
  return true;
}

void NigoriSyncBridgeImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_.passphrase_type) {
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::UNKNOWN:
      NOTREACHED();
      return;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      // Attempt to set the explicit passphrase when one was already set. It's
      // possible if we received new NigoriSpecifics during the passphrase
      // setup.
      DVLOG(1) << "Attempt to set explicit passphrase failed, because one was "
                  "already set.";
      // TODO(crbug.com/922900): investigate whether we need to call
      // OnPassphraseRequired() to prompt for decryption passphrase.
      return;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      break;
  }
  DCHECK(state_.cryptographer->CanEncrypt());

  const KeyDerivationParams custom_passphrase_key_derivation_params =
      CreateKeyDerivationParamsForCustomPassphrase(random_salt_generator_);

  const std::string default_key_name = state_.cryptographer->EmplaceKey(
      passphrase, custom_passphrase_key_derivation_params);
  if (default_key_name.empty()) {
    DLOG(ERROR) << "Failed to set encryption passphrase";
    return;
  }

  state_.cryptographer->SelectDefaultEncryptionKey(default_key_name);
  state_.passphrase_type = NigoriSpecifics::CUSTOM_PASSPHRASE;
  state_.custom_passphrase_key_derivation_params =
      custom_passphrase_key_derivation_params;

  state_.encrypt_everything = true;
  state_.custom_passphrase_time = base::Time::Now();
  processor_->Put(GetData());
  storage_->StoreData(SerializeAsNigoriLocalData());
  for (auto& observer : observers_) {
    observer.OnPassphraseAccepted();
  }
  for (auto& observer : observers_) {
    observer.OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                     state_.custom_passphrase_time);
  }
  for (auto& observer : observers_) {
    observer.OnCryptographerStateChanged(state_.cryptographer.get(),
                                         state_.pending_keys.has_value());
  }
  for (auto& observer : observers_) {
    observer.OnEncryptedTypesChanged(EncryptableUserTypes(),
                                     state_.encrypt_everything);
  }
  MaybeNotifyBootstrapTokenUpdated();
  UMA_HISTOGRAM_BOOLEAN("Sync.CustomEncryption", true);
  // OnLocalSetPassphraseEncryption() is intentionally not called here, because
  // it's needed only for the Directory implementation unit tests.
}

void NigoriSyncBridgeImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |passphrase| should be a valid one already (verified by SyncServiceCrypto,
  // using pending keys exposed by OnPassphraseRequired()).
  DCHECK(!passphrase.empty());
  DCHECK(state_.pending_keys);

  const std::string new_key_name = state_.cryptographer->EmplaceKey(
      passphrase, GetKeyDerivationParamsForPendingKeys());
  if (new_key_name.empty()) {
    processor_->ReportError(ModelError(
        FROM_HERE, "Failed to add decryption passphrase to cryptographer."));
    return;
  }

  sync_pb::NigoriKeyBag decrypted_pending_keys;
  if (!state_.cryptographer->Decrypt(*state_.pending_keys,
                                     &decrypted_pending_keys)) {
    // TODO(crbug.com/922900): old implementation assumes that pending keys
    // encryption key may change in between of OnPassphraseRequired() and
    // SetDecryptionPassphrase() calls, verify whether it's really possible.
    // Hypothetical cases are transition from FROZEN_IMPLICIT_PASSPHRASE to
    // CUSTOM_PASSPHRASE and changing of passphrase due to conflict resolution.
    processor_->ReportError(ModelError(
        FROM_HERE,
        "Failed to decrypt pending keys with provided explicit passphrase."));
    return;
  }

  state_.cryptographer->EmplaceKeysFrom(
      NigoriKeyBag::CreateFromProto(decrypted_pending_keys));
  state_.cryptographer->SelectDefaultEncryptionKey(new_key_name);
  state_.pending_keys.reset();

  storage_->StoreData(SerializeAsNigoriLocalData());
  for (auto& observer : observers_) {
    observer.OnCryptographerStateChanged(state_.cryptographer.get(),
                                         state_.pending_keys.has_value());
  }
  for (auto& observer : observers_) {
    observer.OnPassphraseAccepted();
  }
  MaybeNotifyBootstrapTokenUpdated();
  // TODO(crbug.com/922900): we may need to rewrite encryption_keybag in Nigori
  // node in case we have some keys in |cryptographer_| which is not stored in
  // encryption_keybag yet.
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::EnableEncryptEverything() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

bool NigoriSyncBridgeImpl::IsEncryptEverythingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

base::Time NigoriSyncBridgeImpl::GetKeystoreMigrationTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_.keystore_migration_time;
}

KeystoreKeysHandler* NigoriSyncBridgeImpl::GetKeystoreKeysHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

bool NigoriSyncBridgeImpl::NeedKeystoreKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We explicitly ask the server for keystore keys iff it's first-time sync,
  // i.e. if we have no keystore keys yet. In case of key rotation, it's a
  // server responsibility to send updated keystore keys. |keystore_keys_| is
  // expected to be non-empty before MergeSyncData() call, regardless of
  // passphrase type.
  return state_.keystore_keys.empty();
}

bool NigoriSyncBridgeImpl::SetKeystoreKeys(
    const std::vector<std::string>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (keys.empty() || keys.back().empty()) {
    return false;
  }

  state_.keystore_keys.resize(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    // We need to apply base64 encoding before using the keys to provide
    // backward compatibility with non-USS implementation. It's actually needed
    // only for the keys persisting, but was applied before passing keys to
    // cryptographer, so we have to do the same.
    base::Base64Encode(keys[i], &state_.keystore_keys[i]);
  }

  // Note: we don't need to persist keystore keys here, because we will receive
  // Nigori node right after this method and persist all the data during
  // UpdateLocalState().
  // TODO(crbug.com/922900): support key rotation.
  // TODO(crbug.com/922900): verify that this method is always called before
  // update or init of Nigori node. If this is the case we don't need to touch
  // cryptographer here. If this is not the case, old code is actually broken:
  // 1. Receive and persist the Nigori node after key rotation on different
  // client.
  // 2. Browser crash.
  // 3. After load we don't request new Nigori node from the server (we already
  // have the newest one), so logic with simultaneous sending of Nigori node
  // and keystore keys doesn't help. We don't request new keystore keys
  // explicitly (we already have one). We can't decrypt and use Nigori node
  // with old keystore keys.
  NOTIMPLEMENTED();
  return true;
}

base::Optional<ModelError> NigoriSyncBridgeImpl::MergeSyncData(
    base::Optional<EntityData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data) {
    return ModelError(FROM_HERE,
                      "Received empty EntityData during initial "
                      "sync of Nigori.");
  }
  DCHECK(data->specifics.has_nigori());
  // Ensure we have |keystore_keys| during the initial download, requested to
  // the server as per NeedKeystoreKey(), and required for decrypting the
  // keystore_decryptor_token in specifics or initializing the default keystore
  // Nigori.
  if (state_.keystore_keys.empty()) {
    // TODO(crbug.com/922900): verify, whether it's a valid behavior for custom
    // passphrase.
    return ModelError(FROM_HERE,
                      "Keystore keys are not set during first time sync.");
  }
  if (!data->specifics.nigori().encryption_keybag().blob().empty()) {
    // We received regular Nigori.
    return UpdateLocalState(data->specifics.nigori());
  }
  // We received uninitialized Nigori and need to initialize it as default
  // keystore Nigori.
  base::Optional<NigoriSpecifics> initialized_specifics =
      MakeDefaultKeystoreNigori(state_.keystore_keys);
  if (!initialized_specifics) {
    return ModelError(FROM_HERE, "Failed to initialize keystore Nigori.");
  }
  *data->specifics.mutable_nigori() = *initialized_specifics;
  processor_->Put(std::make_unique<EntityData>(std::move(*data)));
  return UpdateLocalState(*initialized_specifics);
}

base::Optional<ModelError> NigoriSyncBridgeImpl::ApplySyncChanges(
    base::Optional<EntityData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_.passphrase_type, NigoriSpecifics::UNKNOWN);
  if (!data) {
    // Receiving empty |data| means metadata-only change, we need to persist
    // its state.
    storage_->StoreData(SerializeAsNigoriLocalData());
    return base::nullopt;
  }
  DCHECK(data->specifics.has_nigori());
  return UpdateLocalState(data->specifics.nigori());
}

base::Optional<ModelError> NigoriSyncBridgeImpl::UpdateLocalState(
    const NigoriSpecifics& specifics) {
  if (!IsValidNigoriSpecifics(specifics)) {
    return ModelError(FROM_HERE, "NigoriSpecifics is not valid.");
  }

  const sync_pb::NigoriSpecifics::PassphraseType new_passphrase_type =
      ProtoPassphraseInt32ToProtoEnum(specifics.passphrase_type());
  DCHECK_NE(new_passphrase_type, NigoriSpecifics::UNKNOWN);

  if (!IsValidPassphraseTransition(
          /*old_passphrase_type=*/state_.passphrase_type,
          new_passphrase_type)) {
    return ModelError(FROM_HERE, "Invalid passphrase type transition.");
  }
  if (!IsValidEncryptedTypesTransition(state_.encrypt_everything, specifics)) {
    return ModelError(FROM_HERE, "Invalid encrypted types transition.");
  }

  const bool passphrase_type_changed =
      UpdatePassphraseType(new_passphrase_type, &state_.passphrase_type);
  DCHECK_NE(state_.passphrase_type, NigoriSpecifics::UNKNOWN);

  const bool encrypted_types_changed =
      UpdateEncryptedTypes(specifics, &state_.encrypt_everything);

  if (specifics.has_custom_passphrase_time()) {
    state_.custom_passphrase_time =
        ProtoTimeToTime(specifics.custom_passphrase_time());
  }
  if (specifics.has_keystore_migration_time()) {
    state_.keystore_migration_time =
        ProtoTimeToTime(specifics.keystore_migration_time());
  }

  DCHECK(!state_.keystore_keys.empty());
  const sync_pb::EncryptedData& encryption_keybag =
      specifics.encryption_keybag();
  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      // NigoriSpecifics with UNKNOWN or IMPLICIT_PASSPHRASE type is not valid
      // and shouldn't reach this codepath. We just updated |passphrase_type_|
      // from specifics, so it can't be in these states as well.
      NOTREACHED();
      break;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE: {
      base::Optional<ModelError> error = UpdateCryptographerFromKeystoreNigori(
          encryption_keybag, specifics.keystore_decryptor_token());
      if (error) {
        return error;
      }
      break;
    }
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      state_.custom_passphrase_key_derivation_params =
          GetKeyDerivationParamsFromSpecifics(specifics);
      FALLTHROUGH;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      UpdateCryptographerFromNonKeystoreNigori(encryption_keybag);
  }

  storage_->StoreData(SerializeAsNigoriLocalData());
  if (passphrase_type_changed) {
    for (auto& observer : observers_) {
      observer.OnPassphraseTypeChanged(
          *ProtoPassphraseInt32ToEnum(state_.passphrase_type),
          GetExplicitPassphraseTime());
    }
  }
  if (encrypted_types_changed) {
    // Currently the only way to change encrypted types is to enable
    // encrypt_everything.
    DCHECK(state_.encrypt_everything);
    for (auto& observer : observers_) {
      observer.OnEncryptedTypesChanged(EncryptableUserTypes(),
                                       state_.encrypt_everything);
    }
  }
  for (auto& observer : observers_) {
    observer.OnCryptographerStateChanged(state_.cryptographer.get(),
                                         state_.pending_keys.has_value());
  }
  if (state_.pending_keys.has_value()) {
    // Update with keystore Nigori shouldn't reach this point, since it should
    // report model error if it has pending keys.
    for (auto& observer : observers_) {
      observer.OnPassphraseRequired(REASON_DECRYPTION,
                                    GetKeyDerivationParamsForPendingKeys(),
                                    *state_.pending_keys);
    }
  }
  return base::nullopt;
}

base::Optional<ModelError>
NigoriSyncBridgeImpl::UpdateCryptographerFromKeystoreNigori(
    const sync_pb::EncryptedData& encryption_keybag,
    const sync_pb::EncryptedData& keystore_decryptor_token) {
  DCHECK(!encryption_keybag.blob().empty());
  DCHECK(!keystore_decryptor_token.blob().empty());

  state_.cryptographer->EmplaceKeysFrom(DecryptKeystoreDecryptorToken(
      state_.keystore_keys, keystore_decryptor_token));

  sync_pb::NigoriKeyBag key_bag;
  if (!state_.cryptographer->Decrypt(encryption_keybag, &key_bag)) {
    return ModelError(FROM_HERE, "Failed to decrypt incoming keystore nigori.");
  }

  state_.cryptographer->EmplaceKeysFrom(NigoriKeyBag::CreateFromProto(key_bag));
  state_.pending_keys.reset();
  state_.cryptographer->SelectDefaultEncryptionKey(
      encryption_keybag.key_name());
  return base::nullopt;
}

void NigoriSyncBridgeImpl::UpdateCryptographerFromNonKeystoreNigori(
    const sync_pb::EncryptedData& encryption_keybag) {
  // TODO(crbug.com/922900): support the case when client knows passphrase.
  NOTIMPLEMENTED();
  DCHECK(!encryption_keybag.blob().empty());
  if (!state_.cryptographer->CanDecrypt(encryption_keybag)) {
    // Historically, prior to USS, key derived from explicit passphrase was
    // stored in prefs and effectively we do migration here.
    NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
    const std::string key_name =
        key_bag.AddKeyFromProto(explicit_passphrase_key_);
    if (key_bag.CanDecrypt(encryption_keybag)) {
      state_.cryptographer->EmplaceKeysFrom(key_bag);
      DCHECK(state_.cryptographer->CanDecrypt(encryption_keybag));
      state_.cryptographer->SelectDefaultEncryptionKey(key_name);
    } else {
      // This will lead to OnPassphraseRequired() call later.
      state_.pending_keys = encryption_keybag;
      state_.cryptographer->ClearDefaultEncryptionKey();
      return;
    }
  }
  // |cryptographer_| can already have explicit passphrase, in that case it
  // should be able to decrypt |encryption_keybag|. We need to take keys from
  // |encryption_keybag| since some other client can write old keys to
  // |encryption_keybag| and could encrypt some data with them.
  // TODO(crbug.com/922900): find and document at least one real case
  // corresponding to the sentence above.
  // TODO(crbug.com/922900): we may also need to rewrite Nigori with keys
  // currently stored in cryptographer, in case it doesn't have them already.
  sync_pb::NigoriKeyBag key_bag;
  state_.cryptographer->Decrypt(encryption_keybag, &key_bag);
  state_.cryptographer->EmplaceKeysFrom(NigoriKeyBag::CreateFromProto(key_bag));
}

std::unique_ptr<EntityData> NigoriSyncBridgeImpl::GetData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_.passphrase_type == NigoriSpecifics::UNKNOWN) {
    // Bridge never received NigoriSpecifics from the server. This line should
    // be reachable only from processor's GetAllNodesForDebugging().
    DCHECK(!state_.cryptographer->CanEncrypt());
    DCHECK(!state_.pending_keys.has_value());
    return nullptr;
  }

  NigoriSpecifics specifics;
  if (state_.cryptographer->CanEncrypt()) {
    EncryptKeyBag(*state_.cryptographer, specifics.mutable_encryption_keybag());
  } else {
    DCHECK(state_.pending_keys.has_value());
    // This case is reachable only from processor's GetAllNodesForDebugging(),
    // since currently commit is never issued while bridge has |pending_keys_|.
    // Note: with complete support of TRUSTED_VAULT mode, commit might be
    // issued in this case as well.
    *specifics.mutable_encryption_keybag() = *state_.pending_keys;
  }
  specifics.set_keybag_is_frozen(true);
  specifics.set_encrypt_everything(state_.encrypt_everything);
  if (state_.encrypt_everything) {
    UpdateNigoriSpecificsFromEncryptedTypes(EncryptableUserTypes(), &specifics);
  }
  specifics.set_passphrase_type(state_.passphrase_type);
  if (state_.passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE) {
    DCHECK(state_.custom_passphrase_key_derivation_params);
    UpdateSpecificsFromKeyDerivationParams(
        *state_.custom_passphrase_key_derivation_params, &specifics);
  }
  if (state_.passphrase_type == NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    EncryptKeystoreDecryptorToken(*state_.cryptographer,
                                  specifics.mutable_keystore_decryptor_token());
  }
  if (!state_.keystore_migration_time.is_null()) {
    specifics.set_keystore_migration_time(
        TimeToProtoTime(state_.keystore_migration_time));
  }
  if (!state_.custom_passphrase_time.is_null()) {
    specifics.set_custom_passphrase_time(
        TimeToProtoTime(state_.custom_passphrase_time));
  }
  // TODO(crbug.com/922900): add other fields support.
  NOTIMPLEMENTED();
  DCHECK(IsValidNigoriSpecifics(specifics));

  auto entity_data = std::make_unique<EntityData>();
  *entity_data->specifics.mutable_nigori() = std::move(specifics);
  entity_data->name = kNigoriNonUniqueName;
  entity_data->is_folder = true;
  return entity_data;
}

ConflictResolution NigoriSyncBridgeImpl::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return ConflictResolution::kUseLocal;
}

void NigoriSyncBridgeImpl::ApplyDisableSyncChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The user intended to disable sync, so we need to clear all the data, except
  // |explicit_passphrase_key_| in memory, because this function can be called
  // due to BackendMigrator. It's safe even if this function called due to
  // actual disabling sync, since we remove the persisted key by clearing sync
  // prefs explicitly, and don't reuse current instance of the bridge after
  // that.
  // TODO(crbug.com/922900): idea with keeping
  // |explicit_passphrase_key_| will become not working, once we clean up
  // storing explicit passphrase key in prefs, we need to find better solution.
  storage_->ClearData();
  state_.keystore_keys.clear();
  state_.cryptographer = CryptographerImpl::CreateEmpty();
  state_.pending_keys.reset();
  state_.passphrase_type = NigoriSpecifics::UNKNOWN;
  state_.encrypt_everything = false;
  state_.custom_passphrase_time = base::Time();
  state_.keystore_migration_time = base::Time();
  state_.custom_passphrase_key_derivation_params = base::nullopt;
  for (auto& observer : observers_) {
    observer.OnCryptographerStateChanged(state_.cryptographer.get(),
                                         /*has_pending_keys=*/false);
  }
  for (auto& observer : observers_) {
    observer.OnEncryptedTypesChanged(SensitiveTypes(), false);
  }
}

const Cryptographer& NigoriSyncBridgeImpl::GetCryptographerForTesting() const {
  return *state_.cryptographer;
}

sync_pb::NigoriSpecifics::PassphraseType
NigoriSyncBridgeImpl::GetPassphraseTypeForTesting() const {
  return state_.passphrase_type;
}

ModelTypeSet NigoriSyncBridgeImpl::GetEncryptedTypesForTesting() const {
  return GetEncryptedTypes(state_.encrypt_everything);
}

bool NigoriSyncBridgeImpl::HasPendingKeysForTesting() const {
  return state_.pending_keys.has_value();
}

std::string NigoriSyncBridgeImpl::PackExplicitPassphraseKeyForTesting(
    const Encryptor& encryptor,
    const CryptographerImpl& cryptographer) {
  return PackExplicitPassphraseKey(encryptor, cryptographer);
}

base::Time NigoriSyncBridgeImpl::GetExplicitPassphraseTime() const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      // IMPLICIT_PASSPHRASE type isn't supported and should be never set as
      // |passphrase_type_|;
      NOTREACHED();
      return base::Time();
    case NigoriSpecifics::UNKNOWN:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return base::Time();
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      return state_.keystore_migration_time;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      return state_.custom_passphrase_time;
  }
  NOTREACHED();
  return state_.custom_passphrase_time;
}

KeyDerivationParams NigoriSyncBridgeImpl::GetKeyDerivationParamsForPendingKeys()
    const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      NOTREACHED();
      return KeyDerivationParams::CreateWithUnsupportedMethod();
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return KeyDerivationParams::CreateForPbkdf2();
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      DCHECK(state_.custom_passphrase_key_derivation_params);
      return *state_.custom_passphrase_key_derivation_params;
  }
}

void NigoriSyncBridgeImpl::MaybeNotifyBootstrapTokenUpdated() const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      NOTREACHED();
      return;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      // TODO(crbug.com/922900): notify about keystore bootstrap token updates.
      NOTIMPLEMENTED();
      return;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      // This may be problematic for the MIGRATION_DONE case because the local
      // keybag will be cleared and it won't be automatically recovered from
      // prefs.
      NOTIMPLEMENTED();
      return;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      // |packed_custom_passphrase_key| will be empty in case serialization or
      // encryption error occurs.
      std::string packed_custom_passphrase_key =
          PackExplicitPassphraseKey(*encryptor_, *state_.cryptographer);
      if (!packed_custom_passphrase_key.empty()) {
        for (auto& observer : observers_) {
          observer.OnBootstrapTokenUpdated(packed_custom_passphrase_key,
                                           PASSPHRASE_BOOTSTRAP_TOKEN);
        }
      }
  }
}

sync_pb::NigoriLocalData NigoriSyncBridgeImpl::SerializeAsNigoriLocalData()
    const {
  sync_pb::NigoriLocalData output;

  // Serialize the metadata.
  const NigoriMetadataBatch metadata_batch = processor_->GetMetadata();
  *output.mutable_model_type_state() = metadata_batch.model_type_state;
  if (metadata_batch.entity_metadata) {
    *output.mutable_entity_metadata() = *metadata_batch.entity_metadata;
  }

  // Serialize the data.
  *output.mutable_nigori_model() = state_.ToProto();

  return output;
}

}  // namespace syncer
