// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/passwords/password_base_dialog_controller.h"

// An interface used by the credential leak dialog for setting and retrieving
// the state.
class CredentialLeakDialogController : public PasswordBaseDialogController {
 public:
  // Called when user clicks "Check passwords" in the credential leak dialog.
  virtual void OnCheckPasswords() = 0;

  // Called when the dialog was closed by user clicking one of the buttons.
  virtual void OnCloseDialog() = 0;

  // Returns the label for the accept button.
  virtual base::string16 GetAcceptButtonLabel() const = 0;

  // Returns the label for the close button.
  virtual base::string16 GetCloseButtonLabel() const = 0;

  // Returns the dialog message based on credential leak type.
  virtual base::string16 GetDescription() const = 0;

  // Returns the dialog title based on credential leak type.
  virtual base::string16 GetTitle() const = 0;

  // Checks whether the dialog should prompt user to password checkup.
  virtual bool ShouldCheckPasswords() const = 0;

  // Checks whether the dialog should show close button.
  virtual bool ShouldShowCloseButton() const = 0;

 protected:
  ~CredentialLeakDialogController() override = default;
};

#endif  //  CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_
