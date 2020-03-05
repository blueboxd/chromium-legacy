// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credential_leak_password_change_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/passwords/credential_leak_dialog_password_change_view_android.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "ui/android/window_android.h"

using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;

CredentialLeakPasswordChangeControllerAndroid::
    CredentialLeakPasswordChangeControllerAndroid(
        password_manager::CredentialLeakType leak_type,
        const GURL& origin,
        ui::WindowAndroid* window_android)
    : leak_type_(leak_type), origin_(origin), window_android_(window_android) {}

CredentialLeakPasswordChangeControllerAndroid::
    ~CredentialLeakPasswordChangeControllerAndroid() = default;

void CredentialLeakPasswordChangeControllerAndroid::ShowDialog() {
  dialog_view_.reset(new CredentialLeakDialogPasswordChangeViewAndroid(this));
  dialog_view_->Show(window_android_);
}

void CredentialLeakPasswordChangeControllerAndroid::OnCancelDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kClickedClose);
  delete this;
}

void CredentialLeakPasswordChangeControllerAndroid::OnAcceptDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      ShouldCheckPasswords() ? LeakDialogDismissalReason::kClickedCheckPasswords
                             : LeakDialogDismissalReason::kClickedOk);
  delete this;
}

void CredentialLeakPasswordChangeControllerAndroid::OnCloseDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kNoDirectInteraction);
  delete this;
}

base::string16
CredentialLeakPasswordChangeControllerAndroid::GetAcceptButtonLabel() const {
  return password_manager::GetAcceptButtonLabel(leak_type_);
}

base::string16
CredentialLeakPasswordChangeControllerAndroid::GetCancelButtonLabel() const {
  return password_manager::GetCancelButtonLabel();
}

base::string16 CredentialLeakPasswordChangeControllerAndroid::GetDescription()
    const {
  return password_manager::GetDescription(leak_type_, origin_);
}

base::string16 CredentialLeakPasswordChangeControllerAndroid::GetTitle() const {
  return password_manager::GetTitle(leak_type_);
}

bool CredentialLeakPasswordChangeControllerAndroid::ShouldCheckPasswords()
    const {
  return password_manager::ShouldCheckPasswords(leak_type_);
}

bool CredentialLeakPasswordChangeControllerAndroid::ShouldShowCancelButton()
    const {
  return password_manager::ShouldShowCancelButton(leak_type_);
}
