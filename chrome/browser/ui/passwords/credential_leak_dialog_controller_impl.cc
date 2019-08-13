// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include "base/logging.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"

CredentialLeakDialogControllerImpl::CredentialLeakDialogControllerImpl()
    : credential_leak_dialog_(nullptr) {}

CredentialLeakDialogControllerImpl::~CredentialLeakDialogControllerImpl() =
    default;

void CredentialLeakDialogControllerImpl::ShowCredentialLeakPrompt(
    CredentialLeakPrompt* dialog) {
  DCHECK(dialog);
  credential_leak_dialog_ = dialog;
  credential_leak_dialog_->ShowCredentialLeakPrompt();
}

bool CredentialLeakDialogControllerImpl::IsShowingAccountChooser() const {
  return false;
}
