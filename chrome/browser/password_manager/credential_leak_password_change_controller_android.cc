// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credential_leak_password_change_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/PasswordChangeLauncher_jni.h"
#include "chrome/browser/ui/android/passwords/credential_leak_dialog_password_change_view_android.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;

CredentialLeakPasswordChangeControllerAndroid::
    CredentialLeakPasswordChangeControllerAndroid(
        password_manager::CredentialLeakType leak_type,
        const GURL& origin,
        const base::string16& username,
        ui::WindowAndroid* window_android)
    : leak_type_(leak_type),
      origin_(origin),
      username_(username),
      window_android_(window_android) {}

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
  if (window_android_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PasswordChangeLauncher_start(
        env, window_android_->GetJavaObject(),
        base::android::ConvertUTF8ToJavaString(env, origin_.spec()),
        base::android::ConvertUTF16ToJavaString(env, username_));
  }
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
  if (ShouldShowChangePasswordButton()) {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_CHANGE);
  }
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
  return password_manager::ShouldShowCancelButton(leak_type_) ||
         ShouldShowChangePasswordButton();
}

bool CredentialLeakPasswordChangeControllerAndroid::
    ShouldShowChangePasswordButton() const {
  return password_manager::IsPasswordSaved(leak_type_) &&
         !password_manager::IsPasswordUsedOnOtherSites(leak_type_) &&
         password_manager::IsSyncingPasswordsNormally(leak_type_);
}
