// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"

class CredentialLeakPrompt;
class PasswordsLeakDialogDelegate;

// A UI controller responsible for the credential leak dialog.
class CredentialLeakDialogControllerImpl
    : public CredentialLeakDialogController {
 public:
  explicit CredentialLeakDialogControllerImpl(
      PasswordsLeakDialogDelegate* delegate);
  ~CredentialLeakDialogControllerImpl() override;

  // Pop up the credential leak dialog.
  void ShowCredentialLeakPrompt(CredentialLeakPrompt* dialog);

  // CredentialLeakDialogController:
  bool IsShowingAccountChooser() const override;
  void OnCheckPasswords() override;
  void OnCloseDialog() override;

 private:
  // Release |credential_leak_dialog_| and close the open dialog.
  void ResetDialog();

  CredentialLeakPrompt* credential_leak_dialog_;
  PasswordsLeakDialogDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CredentialLeakDialogControllerImpl);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_
