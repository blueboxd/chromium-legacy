// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

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

ArchivableCredential* CredentialFromForm(const PasswordForm& form) {
  DCHECK(!form.blacklisted_by_user);
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
  // TODO(crbug.com/1066803): Wait for things to settle down before sync, and
  // sync credentials after Sync finishes or some seconds in the future.
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
    SaveCredential(*form);
  }
  SyncStore(^(NSError* error) {
    if (!error) {
      [app_group::GetGroupUserDefaults()
          setBool:YES
           forKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
    }
  });
}

void CredentialProviderService::SaveCredential(const PasswordForm& form) const {
  ArchivableCredential* credential = CredentialFromForm(form);
  if (credential) {
    [archivable_credential_store_ addCredential:credential];
  }
}

void CredentialProviderService::UpdateCredential(
    const PasswordForm& form) const {
  ArchivableCredential* credential = CredentialFromForm(form);
  if (credential) {
    [archivable_credential_store_ updateCredential:credential];
  }
}

void CredentialProviderService::RemoveCredential(
    const PasswordForm& form) const {
  ArchivableCredential* credential = CredentialFromForm(form);
  if (credential) {
    [archivable_credential_store_ removeCredential:credential];
  }
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
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        SaveCredential(change.form());
        break;
      case PasswordStoreChange::UPDATE:
        UpdateCredential(change.form());
        break;
      case PasswordStoreChange::REMOVE:
        RemoveCredential(change.form());
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  SyncStore(nil);
}
