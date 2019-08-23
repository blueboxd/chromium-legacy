// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credential_leak_controller_android.h"

#include "chrome/browser/ui/android/passwords/credential_leak_dialog_view_android.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_utils.h"
#include "ui/android/window_android.h"

CredentialLeakControllerAndroid::CredentialLeakControllerAndroid(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin)
    : leak_type_(leak_type), origin_(origin) {}

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

base::string16 CredentialLeakControllerAndroid::GetAcceptButtonLabel() const {
  return leak_dialog_utils::GetAcceptButtonLabel(leak_type_);
}

base::string16 CredentialLeakControllerAndroid::GetCancelButtonLabel() const {
  return leak_dialog_utils::GetCancelButtonLabel();
}

base::string16 CredentialLeakControllerAndroid::GetDescription() const {
  return leak_dialog_utils::GetDescription(leak_type_, origin_);
}

base::string16 CredentialLeakControllerAndroid::GetTitle() const {
  return leak_dialog_utils::GetTitle(leak_type_);
}

bool CredentialLeakControllerAndroid::ShouldCheckPasswords() const {
  return leak_dialog_utils::ShouldCheckPasswords(leak_type_);
}

bool CredentialLeakControllerAndroid::ShouldShowCancelButton() const {
  return leak_dialog_utils::ShouldShowCancelButton(leak_type_);
}
