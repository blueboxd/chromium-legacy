// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"

#if !defined(NDEBUG)
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#endif  // !defined(NDEBUG)

namespace {

using autofill::PasswordForm;
using password_manager::PasswordStore;

BOOL ShouldSyncAllCredentials() {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  DCHECK(shared_defaults);
  return ![shared_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
}

}  // namespace

CredentialProviderService::CredentialProviderService(
    scoped_refptr<PasswordStore> password_store,
    NSURL* file_url)
    : password_store_(password_store) {
  DCHECK(password_store_);
  archivable_credential_store_ =
      [[ArchivableCredentialStore alloc] initWithFileURL:file_url];
  // TODO(crbug.com/1066803): Wait for things to settle down before sync, and
  // sync credentials after Sync finishes or some seconds in the future.
  if (ShouldSyncAllCredentials()) {
    SyncAllCredentials();
  }
}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {}

void CredentialProviderService::SyncAllCredentials() {
  password_store_->GetAutofillableLogins(this);
}

void CredentialProviderService::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  [archivable_credential_store_ removeAllCredentials];
  for (const auto& form : results) {
    SaveCredential(*form);
  }
  SyncStore();
}

void CredentialProviderService::SaveCredential(const PasswordForm& form) const {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithPasswordForm:form
                                                 favicon:nil
                                    validationIdentifier:nil];
  if (credential) {
    [archivable_credential_store_ addCredential:credential];
  }
#if !defined(NDEBUG)
  // Double check that the credential is nil because it is an Android one.
  else {
    DCHECK(password_manager::IsValidAndroidFacetURI(form.signon_realm));
  }
#endif  // !defined(NDEBUG)
}

void CredentialProviderService::SyncStore() const {
  [archivable_credential_store_ saveDataWithCompletion:^(NSError* error) {
    DCHECK(!error) << "An error occurred while saving to disk";
    if (!error) {
      [app_group::GetGroupUserDefaults()
          setBool:YES
           forKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
    }
  }];
}
