// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_update_delegate.h"

#include "base/stl_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "components/password_manager/core/browser/password_store.h"

PasswordUpdateDelegate::PasswordUpdateDelegate(
    Profile* profile,
    const autofill::PasswordForm& password_form)
    : profile_(profile), password_form_(password_form) {}

PasswordUpdateDelegate::~PasswordUpdateDelegate() = default;

void PasswordUpdateDelegate::EditSavedPassword(
    const base::string16& new_username,
    const base::string16& new_password) {
  DCHECK(!new_password.empty()) << "The password is empty.";

  new_username_ = new_username;
  new_password_ = new_password;
  PasswordStoreFactory::GetForProfile(profile_,
                                      ServiceAccessType::EXPLICIT_ACCESS)
      ->GetLogins(password_manager::PasswordStore::FormDigest(password_form_),
                  this);
}

void PasswordUpdateDelegate::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  // Banned password credentials and the ones with invalid origin won't be
  // edited through this delegate.
  base::EraseIf(results, [](const auto& form) {
    return form->blacklisted_by_user || form->IsFederatedCredential();
  });

  EditSavedPasswords(profile_, results, password_form_.username_value,
                     password_form_.signon_realm, new_username_,
                     &new_password_);
}
