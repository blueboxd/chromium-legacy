// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_error.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/nigori/keystore_keys_handler.h"
#include "components/sync/nigori/nigori_local_change_processor.h"
#include "components/sync/nigori/nigori_sync_bridge.h"

namespace syncer {

class Encryptor;

// USS implementation of SyncEncryptionHandler.
// This class holds the current Nigori state and processes incoming changes and
// queries:
// 1. Serves observers of SyncEncryptionHandler interface.
// 2. Allows the passphrase manipulations (via SyncEncryptionHandler).
// 3. Communicates local and remote changes with a processor (via
// NigoriSyncBridge).
// 4. Handles keystore keys from a sync server (via KeystoreKeysHandler).
class NigoriSyncBridgeImpl : public KeystoreKeysHandler,
                             public NigoriSyncBridge,
                             public SyncEncryptionHandler {
 public:
  // |encryptor| must be not null and must outlive this object.
  NigoriSyncBridgeImpl(std::unique_ptr<NigoriLocalChangeProcessor> processor,
                       const Encryptor* encryptor);
  ~NigoriSyncBridgeImpl() override;

  // SyncEncryptionHandler implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool Init() override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  void SetDecryptionPassphrase(const std::string& passphrase) override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  base::Time GetKeystoreMigrationTime() const override;
  Cryptographer* GetCryptographerUnsafe() override;
  KeystoreKeysHandler* GetKeystoreKeysHandler() override;
  syncable::NigoriHandler* GetNigoriHandler() override;

  // KeystoreKeysHandler implementation.
  bool NeedKeystoreKey() const override;
  bool SetKeystoreKeys(const std::vector<std::string>& keys) override;

  // NigoriSyncBridge implementation.
  base::Optional<ModelError> MergeSyncData(
      base::Optional<EntityData> data) override;
  base::Optional<ModelError> ApplySyncChanges(
      base::Optional<EntityData> data) override;
  std::unique_ptr<EntityData> GetData() override;
  ConflictResolution ResolveConflict(const EntityData& local_data,
                                     const EntityData& remote_data) override;
  void ApplyDisableSyncChanges() override;

  // TODO(crbug.com/922900): investigate whether we need this getter outside of
  // tests and decide whether this method should be a part of
  // SyncEncryptionHandler interface.
  const Cryptographer& GetCryptographerForTesting() const;

 private:
  base::Optional<ModelError> UpdateLocalState(
      const sync_pb::NigoriSpecifics& specifics);

  base::Optional<ModelError> UpdateCryptographerFromKeystoreNigori(
      const sync_pb::EncryptedData& encryption_keybag,
      const sync_pb::EncryptedData& keystore_decryptor_token);

  void UpdateCryptographerFromExplicitPassphraseNigori(
      const sync_pb::EncryptedData& keybag);

  base::Time GetExplicitPassphraseTime() const;

  // Returns key derivation params based on |passphrase_type_| and
  // |custom_passphrase_key_derivation_params_|. Should be called only if
  // |passphrase_type_| is an explicit passphrase.
  KeyDerivationParams GetKeyDerivationParamsForPendingKeys() const;

  // Persists Nigori derived from explicit passphrase into preferences, in case
  // error occurs during serialization/encryption, corresponding preference
  // just won't be updated.
  void MaybeNotifyBootstrapTokenUpdated() const;

  const Encryptor* const encryptor_;

  const std::unique_ptr<NigoriLocalChangeProcessor> processor_;

  // Base64 encoded keystore keys. The last element is the current keystore
  // key. These keys are not a part of Nigori node and are persisted
  // separately. Should be encrypted with OSCrypt before persisting.
  std::vector<std::string> keystore_keys_;

  Cryptographer cryptographer_;
  // TODO(mmoskvitin): Consider adopting the C++ enum PassphraseType here and
  // if so remove function ProtoPassphraseInt32ToProtoEnum() from
  // passphrase_enums.h.
  sync_pb::NigoriSpecifics::PassphraseType passphrase_type_;
  bool encrypt_everything_;
  base::Time custom_passphrase_time_;
  base::Time keystore_migration_time_;

  // The key derivation params we are using for the custom passphrase. Set iff
  // |passphrase_type_| is CUSTOM_PASSPHRASE, otherwise key derivation method
  // is always PBKDF2.
  base::Optional<KeyDerivationParams> custom_passphrase_key_derivation_params_;

  // TODO(crbug/922900): consider using checked ObserverList once
  // SyncEncryptionHandlerImpl is no longer needed or consider refactoring old
  // implementation to use checked ObserverList as well.
  base::ObserverList<SyncEncryptionHandler::Observer>::Unchecked observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NigoriSyncBridgeImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_
