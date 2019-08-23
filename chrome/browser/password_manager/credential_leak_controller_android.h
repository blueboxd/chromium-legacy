// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_LEAK_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_LEAK_CONTROLLER_ANDROID_H_

#include <memory>
#include "base/macros.h"

namespace ui {
class WindowAndroid;
}

class CredentialLeakDialogViewAndroid;

// Class which manages the dialog displayed when a credential leak was
// detected. It is self-owned and it owns the dialog view.
class CredentialLeakControllerAndroid {
 public:
  CredentialLeakControllerAndroid();
  ~CredentialLeakControllerAndroid();

  // Called when a leaked credential was detected.
  void ShowDialog(ui::WindowAndroid* window_android);

  // Called from the UI when the dialog dismissal was requested.
  // Will destroy the controller.
  void OnDialogDismissRequested();

  // Called from the UI when the password check button was pressed.
  // Will destroy the controller.
  void OnPasswordCheckTriggered();

 private:
  std::unique_ptr<CredentialLeakDialogViewAndroid> dialog_view_;

  DISALLOW_COPY_AND_ASSIGN(CredentialLeakControllerAndroid);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_LEAK_CONTROLLER_ANDROID_H_