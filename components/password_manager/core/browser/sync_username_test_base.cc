// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_username_test_base.h"

#include "base/strings/utf_string_conversions.h"

using autofill::PasswordForm;

namespace password_manager {

SyncUsernameTestBase::SyncUsernameTestBase() = default;

SyncUsernameTestBase::~SyncUsernameTestBase() = default;

void SyncUsernameTestBase::FakeSigninAs(const std::string& email) {
  // This method is called in a roll by some tests. IdentityTestEnvironment does
  // not allow logging in without a previously log-out.
  // So make sure tests only log in once and that the email is the same in case
  // of FakeSigninAs calls roll.
  signin::IdentityManager* identity_manager =
      identity_test_env_.identity_manager();
  if (identity_manager->HasPrimaryAccount()) {
    DCHECK_EQ(identity_manager->GetPrimaryAccountInfo().email, email);
  } else {
    identity_test_env_.MakePrimaryAccountAvailable(email);
  }
}

// static
PasswordForm SyncUsernameTestBase::SimpleGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://accounts.google.com";
  form.username_value = base::ASCIIToUTF16(username);
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.username_value = base::ASCIIToUTF16(username);
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username,
                                                     const char* origin) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.username_value = base::ASCIIToUTF16(username);
  form.origin = GURL(origin);
  return form;
}

void SyncUsernameTestBase::SetSyncingPasswords(bool syncing_passwords) {
  sync_service_.SetPreferredDataTypes(
      syncing_passwords ? syncer::ModelTypeSet(syncer::PASSWORDS)
                        : syncer::ModelTypeSet());
}

}  // namespace password_manager
