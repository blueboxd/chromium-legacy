// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password store.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {
class PasswordStore;
}

class Profile;

// Changes a credential record in password store. If new_password is null it
// isn't changed, but if it is non-null it can't be empty.
void EditSavedPasswords(
    Profile* profile,
    const base::span<const std::unique_ptr<autofill::PasswordForm>> old_forms,
    const base::string16& old_username,
    const std::string& signon_realm,
    const base::string16& new_username,
    const base::string16* new_password);

// Returns the password store associated with the currently active profile.
scoped_refptr<password_manager::PasswordStore> GetPasswordStore(
    Profile* profile,
    bool use_account_store);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_
