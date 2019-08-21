// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include "chrome/browser/ui/passwords/credential_leak_dialog_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"

using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;

CredentialLeakDialogControllerImpl::CredentialLeakDialogControllerImpl(
    PasswordsLeakDialogDelegate* delegate,
    CredentialLeakType leak_type,
    const GURL& origin)
    : credential_leak_dialog_(nullptr),
      delegate_(delegate),
      leak_type_(leak_type),
      origin_(origin) {}

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

base::string16 CredentialLeakDialogControllerImpl::GetAcceptButtonLabel()
    const {
  return leak_dialog_utils::GetAcceptButtonLabel(leak_type_);
}

base::string16 CredentialLeakDialogControllerImpl::GetCloseButtonLabel() const {
  return leak_dialog_utils::GetCloseButtonLabel();
}

base::string16 CredentialLeakDialogControllerImpl::GetDescription() const {
  return leak_dialog_utils::GetDescription(leak_type_, origin_);
}

base::string16 CredentialLeakDialogControllerImpl::GetTitle() const {
  return leak_dialog_utils::GetTitle(leak_type_);
}

bool CredentialLeakDialogControllerImpl::ShouldCheckPasswords() const {
  return leak_dialog_utils::ShouldCheckPasswords(leak_type_);
}

bool CredentialLeakDialogControllerImpl::ShouldShowCloseButton() const {
  return leak_dialog_utils::ShouldShowCloseButton(leak_type_);
}

void CredentialLeakDialogControllerImpl::ResetDialog() {
  if (credential_leak_dialog_) {
    credential_leak_dialog_->ControllerGone();
    credential_leak_dialog_ = nullptr;
  }
}
