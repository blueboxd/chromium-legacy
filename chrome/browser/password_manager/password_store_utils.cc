// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_utils.h"

#include "chrome/browser/password_manager/account_storage/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"

void EditSavedPasswords(
    Profile* profile,
    const base::span<const std::unique_ptr<autofill::PasswordForm>> old_forms,
    const base::string16& old_username,
    const std::string& signon_realm,
    const base::string16& new_username,
    const base::string16* new_password) {
  DCHECK(!old_forms.empty());

  const bool username_changed = old_username != new_username;

  // In case the username changed, make sure that there exists no other
  // credential with the same signon_realm and username.
  if (username_changed &&
      std::any_of(old_forms.begin(), old_forms.end(),
                  [&](const auto& old_form) {
                    return old_form->signon_realm == signon_realm &&
                           old_form->username_value == new_username;
                  })) {
    // TODO(crbug.com/1002021): We shouldn't fail silently.
    DLOG(ERROR) << "A credential with the same signon_realm and username "
                   "already exists.";
    return;
  }

  // An updated username implies a change in the primary key, thus we need to
  // make sure to call the right API. Update every entry in the equivalence
  // class.
  for (const auto& old_form : old_forms) {
    scoped_refptr<password_manager::PasswordStore> store =
        GetPasswordStore(profile, old_form->IsUsingAccountStore());

    if (!store)
      continue;
    autofill::PasswordForm new_form = *old_form;
    if (new_password)
      new_form.password_value = *new_password;

    if (username_changed) {
      new_form.username_value = new_username;
      store->UpdateLoginWithPrimaryKey(new_form, *old_form);
    } else {
      store->UpdateLogin(new_form);
    }
  }
}

scoped_refptr<password_manager::PasswordStore> GetPasswordStore(
    Profile* profile,
    bool use_account_store) {
  if (use_account_store) {
    return AccountPasswordStoreFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
  }
  return PasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}
