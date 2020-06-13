// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/authentication_service.h"

@class ArchivableCredentialStore;

// A browser-context keyed service that is used to keep the Credential Provider
// Extension data up to date.
class CredentialProviderService
    : public KeyedService,
      public password_manager::PasswordStoreConsumer,
      public password_manager::PasswordStore::Observer,
      public signin::IdentityManager::Observer {
 public:
  // Initializes the service.
  CredentialProviderService(
      scoped_refptr<password_manager::PasswordStore> password_store,
      AuthenticationService* authentication_service,
      ArchivableCredentialStore* credential_store,
      signin::IdentityManager* identity_manager);
  ~CredentialProviderService() override;

  // KeyedService:
  void Shutdown() override;

  // IdentityManager::Observer.
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

 private:
  // Request all the credentials to sync them. Before adding the fresh ones,
  // the old ones are deleted.
  void RequestSyncAllCredentials();

  // Syncs the credential store to disk.
  void SyncStore(void (^completion)(NSError*)) const;

  // Syncs account_validation_id_.
  void UpdateAccountValidationId();

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // PasswordStore::Observer:
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // The interface for getting the primary account identifier.
  AuthenticationService* authentication_service_ = nullptr;

  // Identity manager to observe.
  signin::IdentityManager* identity_manager_;

  // The interface for saving and updating credentials.
  ArchivableCredentialStore* archivable_credential_store_ = nil;

  // The current validation ID or nil.
  NSString* account_validation_id_ = nil;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderService);
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_
