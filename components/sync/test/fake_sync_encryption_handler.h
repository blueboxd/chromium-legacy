// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_ENCRYPTION_HANDLER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_ENCRYPTION_HANDLER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/nigori/keystore_keys_handler.h"
#include "components/sync/syncable/nigori_handler.h"

namespace syncer {

// A fake sync encryption handler capable of keeping track of the encryption
// state without opening any transactions or interacting with the nigori node.
// Note that this only performs basic interactions with the cryptographer
// (setting pending keys, installing keys).
// Note: NOT thread safe. If threads attempt to check encryption state
// while another thread is modifying it, races can occur.
class FakeSyncEncryptionHandler : public KeystoreKeysHandler,
                                  public SyncEncryptionHandler,
                                  public syncable::NigoriHandler {
 public:
  FakeSyncEncryptionHandler();
  ~FakeSyncEncryptionHandler() override;

  // SyncEncryptionHandler implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool Init() override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  void SetDecryptionPassphrase(const std::string& passphrase) override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  PassphraseType GetPassphraseType(
      syncable::BaseTransaction* const trans) const override;
  base::Time GetKeystoreMigrationTime() const override;
  Cryptographer* GetCryptographerUnsafe() override;
  KeystoreKeysHandler* GetKeystoreKeysHandler() override;
  syncable::NigoriHandler* GetNigoriHandler() override;

  // NigoriHandler implemenation.
  bool ApplyNigoriUpdate(const sync_pb::NigoriSpecifics& nigori,
                         syncable::BaseTransaction* const trans) override;
  void UpdateNigoriFromEncryptedTypes(
      sync_pb::NigoriSpecifics* nigori,
      syncable::BaseTransaction* const trans) const override;
  ModelTypeSet GetEncryptedTypes(
      syncable::BaseTransaction* const trans) const override;

  // KeystoreKeysHandler implementation.
  bool NeedKeystoreKey() const override;
  bool SetKeystoreKeys(const std::vector<std::string>& keys) override;

 private:
  base::ObserverList<SyncEncryptionHandler::Observer>::Unchecked observers_;
  ModelTypeSet encrypted_types_;
  bool encrypt_everything_;
  PassphraseType passphrase_type_;

  Cryptographer cryptographer_;
  std::string keystore_key_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_ENCRYPTION_HANDLER_H_
