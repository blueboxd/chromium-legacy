// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_

#include <string>

#include "components/sync/driver/sync_user_settings.h"

namespace syncer {

class TestSyncService;

// Test implementation of SyncUserSettings that mostly forwards calls to a
// TestSyncService.
class TestSyncUserSettings : public SyncUserSettings {
 public:
  explicit TestSyncUserSettings(TestSyncService* service);
  ~TestSyncUserSettings() override;

  bool IsSyncRequested() const override;
  void SetSyncRequested(bool requested) override;

  bool IsSyncAllowedByPlatform() const override;
  void SetSyncAllowedByPlatform(bool allowed) override;

  bool IsFirstSetupComplete() const override;
  void SetFirstSetupComplete(SyncFirstSetupCompleteSource source) override;

  bool IsSyncEverythingEnabled() const override;
  UserSelectableTypeSet GetSelectedTypes() const override;
  void SetSelectedTypes(bool sync_everything,
                        UserSelectableTypeSet types) override;
  UserSelectableTypeSet GetRegisteredSelectableTypes() const override;
  UserSelectableTypeSet GetForcedTypes() const override;

  bool IsEncryptEverythingAllowed() const override;
  bool IsEncryptEverythingEnabled() const override;
  void EnableEncryptEverything() override;

  syncer::ModelTypeSet GetEncryptedDataTypes() const override;
  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForDecryption() const override;
  bool IsUsingSecondaryPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  PassphraseType GetPassphraseType() const override;

  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;

  void SetFirstSetupComplete();
  void ClearFirstSetupComplete();
  void SetEncryptEverythingAllowed(bool allowed);
  void SetPassphraseRequired(bool required);
  void SetPassphraseRequiredForDecryption(bool required);
  void SetIsUsingSecondaryPassphrase(bool enabled);

 private:
  TestSyncService* service_;

  bool first_setup_complete_ = true;
  bool sync_everything_enabled_ = true;

  bool passphrase_required_ = false;
  bool passphrase_required_for_decryption_ = false;
  bool using_secondary_passphrase_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_
