// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credential_leak_controller_android.h"

#include "chrome/browser/ui/android/passwords/credential_leak_dialog_view_android.h"
#include "ui/android/window_android.h"

CredentialLeakControllerAndroid::CredentialLeakControllerAndroid() = default;

CredentialLeakControllerAndroid::~CredentialLeakControllerAndroid() = default;

void CredentialLeakControllerAndroid::ShowDialog(
    ui::WindowAndroid* window_android) {
  dialog_view_.reset(new CredentialLeakDialogViewAndroid(this));
  dialog_view_->Show(window_android);
}

void CredentialLeakControllerAndroid::OnDialogDismissRequested() {
  delete this;
}

void CredentialLeakControllerAndroid::OnPasswordCheckTriggered() {
  // TODO(crbug.com/986317): Navigate to the password check site.
  delete this;
}
