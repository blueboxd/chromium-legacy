// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_LEAK_PASSWORD_CHANGE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_LEAK_PASSWORD_CHANGE_CONTROLLER_ANDROID_H_

#include <memory>
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace ui {
class WindowAndroid;
}

class CredentialLeakDialogPasswordChangeViewAndroid;

// TODO (crbug/1058764) Unfork credential leak dialog password change when
// prototype is done.

// Class which manages the dialog displayed when a credential leak was
// detected. It is self-owned and it owns the dialog view.
class CredentialLeakPasswordChangeControllerAndroid {
 public:
  CredentialLeakPasswordChangeControllerAndroid(
      password_manager::CredentialLeakType leak_type,
      const GURL& origin,
      const base::string16& username,
      ui::WindowAndroid* window_android);
  ~CredentialLeakPasswordChangeControllerAndroid();

  // Called when a leaked credential was detected.
  void ShowDialog();

  // Called from the UI when the "Close" button was pressed.
  // Will destroy the controller.
  void OnCancelDialog();

  // Called from the UI when the okay or password check button was pressed.
  // Will destroy the controller.
  void OnAcceptDialog();

  // Called from the UI when the dialog was dismissed by other means (e.g. back
  // button).
  // Will destroy the controller.
  void OnCloseDialog();

  // The label of the accept button. Varies by leak type.
  base::string16 GetAcceptButtonLabel() const;

  // The label of the cancel button. Varies by leak type.
  base::string16 GetCancelButtonLabel() const;

  // Text explaining the leak details. Varies by leak type.
  base::string16 GetDescription() const;

  // The title of the dialog displaying the leak warning.
  base::string16 GetTitle() const;

  // Checks whether the dialog should show the option to check passwords.
  bool ShouldCheckPasswords() const;

  // Checks whether the cancel button should be shown.
  bool ShouldShowCancelButton() const;

 private:
  bool ShouldShowChangePasswordButton() const;

  // Used to customize the UI.
  const password_manager::CredentialLeakType leak_type_;

  const GURL origin_;

  const base::string16 username_;

  ui::WindowAndroid* window_android_;

  std::unique_ptr<CredentialLeakDialogPasswordChangeViewAndroid> dialog_view_;

  DISALLOW_COPY_AND_ASSIGN(CredentialLeakPasswordChangeControllerAndroid);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_LEAK_PASSWORD_CHANGE_CONTROLLER_ANDROID_H_
