// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include "base/logging.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"

CredentialLeakDialogControllerImpl::CredentialLeakDialogControllerImpl(
    PasswordsLeakDialogDelegate* delegate)
    : credential_leak_dialog_(nullptr), delegate_(delegate) {}

CredentialLeakDialogControllerImpl::~CredentialLeakDialogControllerImpl() {
  ResetDialog();
}

void CredentialLeakDialogControllerImpl::ShowCredentialLeakPrompt(
    CredentialLeakPrompt* dialog) {
  DCHECK(dialog);
  credential_leak_dialog_ = dialog;
  credential_leak_dialog_->ShowCredentialLeakPrompt();
}

bool CredentialLeakDialogControllerImpl::IsShowingAccountChooser() const {
  return false;
}

void CredentialLeakDialogControllerImpl::OnCheckPasswords() {
  delegate_->NavigateToPasswordCheckup();
  ResetDialog();
  OnCloseDialog();
}

void CredentialLeakDialogControllerImpl::OnCloseDialog() {
  // TODO(crbug.com/986317): Add logging metrics util.
  if (credential_leak_dialog_) {
    credential_leak_dialog_ = nullptr;
  }
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogControllerImpl::ResetDialog() {
  if (credential_leak_dialog_) {
    credential_leak_dialog_->ControllerGone();
    credential_leak_dialog_ = nullptr;
  }
}
