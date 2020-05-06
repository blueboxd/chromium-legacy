// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

#import <AuthenticationServices/AuthenticationServices.h>

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/as_password_credential_identity+credential.h"
#import "ios/chrome/common/credential_provider/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using autofill::PasswordForm;
using password_manager::PasswordStore;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;

BOOL ShouldSyncAllCredentials() {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  DCHECK(shared_defaults);
  return ![shared_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
}

BOOL ShouldSyncASIdentityStore() {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  DCHECK(shared_defaults);
  BOOL isIdentityStoreSynced = [shared_defaults
      boolForKey:kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted];
  BOOL areCredentialsSynced = [shared_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
  return !isIdentityStoreSynced && areCredentialsSynced;
}

void SyncASIdentityStore(ArchivableCredentialStore* credential_store) {
  auto stateCompletion = ^(ASCredentialIdentityStoreState* state) {
#if !defined(NDEBUG)
    dispatch_assert_queue_not(dispatch_get_main_queue());
#endif  // !defined(NDEBUG)
    if (state.enabled) {
      NSArray<id<Credential>>* credentials = credential_store.credentials;
      NSMutableArray<ASPasswordCredentialIdentity*>* storeIdentities =
          [NSMutableArray arrayWithCapacity:credentials.count];
      for (id<Credential> credential in credentials) {
        [storeIdentities addObject:[[ASPasswordCredentialIdentity alloc]
                                       initWithCredential:credential]];
      }
      auto replaceCompletion = ^(BOOL success, NSError* error) {
        DCHECK(success) << "Failed to update store, error: "
                        << error.description;
        NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
        NSString* key =
            kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted;
        [shared_defaults setBool:success forKey:key];
      };
      [ASCredentialIdentityStore.sharedStore
          replaceCredentialIdentitiesWithIdentities:storeIdentities
                                         completion:replaceCompletion];
    }
  };
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:stateCompletion];
}

ArchivableCredential* CredentialFromForm(const PasswordForm& form) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithPasswordForm:form
                                                 favicon:nil
                                    validationIdentifier:nil];
  if (!credential) {
    // Verify that the credential is nil because it's an Android one or
    // blacklisted.
    DCHECK(password_manager::IsValidAndroidFacetURI(form.signon_realm) ||
           form.blacklisted_by_user);
  }
  return credential;
}

}  // namespace

CredentialProviderService::CredentialProviderService(
    scoped_refptr<PasswordStore> password_store,
    ArchivableCredentialStore* credential_store)
    : password_store_(password_store),
      archivable_credential_store_(credential_store) {
  DCHECK(password_store_);
  password_store_->AddObserver(this);
  // TODO(crbug.com/1066803): Wait for things to settle down before syncs, and
  // sync credentials after Sync finishes or some seconds in the future.
  if (ShouldSyncASIdentityStore()) {
    SyncASIdentityStore(credential_store);
  }
  if (ShouldSyncAllCredentials()) {
    RequestSyncAllCredentials();
  }
}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {}

void CredentialProviderService::RequestSyncAllCredentials() {
  password_store_->GetAutofillableLogins(this);
}

void CredentialProviderService::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  [archivable_credential_store_ removeAllCredentials];
  for (const auto& form : results) {
    ArchivableCredential* credential = CredentialFromForm(*form);
    if (credential) {
      [archivable_credential_store_ addCredential:credential];
    }
  }
  SyncStore(^(NSError* error) {
    if (!error) {
      [app_group::GetGroupUserDefaults()
          setBool:YES
           forKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
      SyncASIdentityStore(archivable_credential_store_);
    }
  });
}

void CredentialProviderService::SyncStore(void (^completion)(NSError*)) const {
  [archivable_credential_store_ saveDataWithCompletion:^(NSError* error) {
    DCHECK(!error) << "An error occurred while saving to disk";
    if (completion) {
      completion(error);
    }
  }];
}

void CredentialProviderService::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  for (const PasswordStoreChange& change : changes) {
    ArchivableCredential* credential = CredentialFromForm(change.form());
    if (!credential) {
      continue;
    }
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        [archivable_credential_store_ addCredential:credential];
        break;
      case PasswordStoreChange::UPDATE:
        [archivable_credential_store_ updateCredential:credential];
        break;
      case PasswordStoreChange::REMOVE:
        [archivable_credential_store_ removeCredential:credential];
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  SyncStore(^(NSError* error) {
    if (!error) {
      // TODO(crbug.com/1077747): Support ASCredentialIdentityStore incremental
      // updates. Currently calling multiple methods on it to save and remove
      // causes it to crash. This needs to be further investigated.
      SyncASIdentityStore(archivable_credential_store_);
    }
  });
}
