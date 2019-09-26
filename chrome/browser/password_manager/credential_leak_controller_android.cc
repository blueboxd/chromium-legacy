// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credential_leak_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/PasswordCheckupLauncher_jni.h"
#include "chrome/browser/ui/android/passwords/credential_leak_dialog_view_android.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_utils.h"
#include "chrome/common/url_constants.h"
#include "ui/android/window_android.h"

using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;

CredentialLeakControllerAndroid::CredentialLeakControllerAndroid(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    ui::WindowAndroid* window_android)
    : leak_type_(leak_type), origin_(origin), window_android_(window_android) {}

CredentialLeakControllerAndroid::~CredentialLeakControllerAndroid() = default;

void CredentialLeakControllerAndroid::ShowDialog() {
  dialog_view_.reset(new CredentialLeakDialogViewAndroid(this));
  dialog_view_->Show(window_android_);
}

void CredentialLeakControllerAndroid::OnCancelDialog() {
  LogLeakDialogTypeAndDismissalReason(
      leak_dialog_utils::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kClickedClose);
  delete this;
}

void CredentialLeakControllerAndroid::OnAcceptDialog() {
  LogLeakDialogTypeAndDismissalReason(
      leak_dialog_utils::GetLeakDialogType(leak_type_),
      ShouldCheckPasswords() ? LeakDialogDismissalReason::kClickedCheckPasswords
                             : LeakDialogDismissalReason::kClickedOk);

  // |window_android_| might be null in tests.
  if (window_android_ && ShouldCheckPasswords()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PasswordCheckupLauncher_launchCheckup(
        env,
        base::android::ConvertUTF8ToJavaString(
            env, leak_dialog_utils::GetPasswordCheckupURL().spec()),
        window_android_->GetJavaObject());
  }

  delete this;
}

void CredentialLeakControllerAndroid::OnCloseDialog() {
  LogLeakDialogTypeAndDismissalReason(
      leak_dialog_utils::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kNoDirectInteraction);
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

gfx::Range CredentialLeakControllerAndroid::GetDescriptionBoldRange() const {
  return leak_dialog_utils::GetChangePasswordBoldRange(leak_type_, origin_);
}

bool CredentialLeakControllerAndroid::ShouldCheckPasswords() const {
  return leak_dialog_utils::ShouldCheckPasswords(leak_type_);
}

bool CredentialLeakControllerAndroid::ShouldShowCancelButton() const {
  return leak_dialog_utils::ShouldShowCancelButton(leak_type_);
}
