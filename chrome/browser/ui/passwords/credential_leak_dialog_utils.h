// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_UTILS_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace leak_dialog_utils {

// Returns the label for the leak dialog accept button.
base::string16 GetAcceptButtonLabel(
    password_manager::CredentialLeakType leak_type);

// Returns the label for the leak dialog cancel button.
base::string16 GetCancelButtonLabel();

// Returns the leak dialog message based on leak type.
base::string16 GetDescription(password_manager::CredentialLeakType leak_type,
                              const GURL& origin);

// Returns the leak dialog title based on leak type.
base::string16 GetTitle(password_manager::CredentialLeakType leak_type);

// Checks whether the leak dialog should prompt user to password checkup.
bool ShouldCheckPasswords(password_manager::CredentialLeakType leak_type);

// Checks whether the leak dialog should show cancel button.
bool ShouldShowCancelButton(password_manager::CredentialLeakType leak_type);

// Returns the LeakDialogType corresponding to |leak_type|.
password_manager::metrics_util::LeakDialogType GetLeakDialogType(
    password_manager::CredentialLeakType leak_type);

// Returns the URL used to launch the password checkup.
GURL GetPasswordCheckupURL();

// Returns the range of the bold part of the leak dialog message when
// credentials were leaked only on current site.
gfx::Range GetChangePasswordBoldRange(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin);

}  // namespace leak_dialog_utils

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_UTILS_H_
