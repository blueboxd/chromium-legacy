// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

@class ArchivableCredentialStore;

// A browser-context keyed service that is used to keep the Credential Provider
// Extension data up to date.
class CredentialProviderService
    : public KeyedService,
      public password_manager::PasswordStoreConsumer,
      public password_manager::PasswordStore::Observer {
 public:
  // Initializes the service.
  CredentialProviderService(
      scoped_refptr<password_manager::PasswordStore> password_store,
      ArchivableCredentialStore* credential_store);
  ~CredentialProviderService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Request all the credentials to sync them. Before adding the fresh ones,
  // the old ones are deleted.
  void RequestSyncAllCredentials();

  // Syncs the credential store to disk.
  void SyncStore(void (^completion)(NSError*)) const;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // PasswordStore::Observer:
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // The interface for saving and updating credentials.
  ArchivableCredentialStore* archivable_credential_store_ = nil;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderService);
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_
