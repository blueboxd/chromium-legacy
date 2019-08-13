// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_

#include "chrome/browser/ui/passwords/password_base_dialog_controller.h"

// An interface used by the credential leak dialog for setting and retrieving
// the state.
class CredentialLeakDialogController : public PasswordBaseDialogController {
 protected:
  ~CredentialLeakDialogController() override = default;
};

#endif  //  CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_
