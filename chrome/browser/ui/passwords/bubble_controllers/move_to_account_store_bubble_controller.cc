// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/common/password_manager_ui.h"

MoveToAccountStoreBubbleController::MoveToAccountStoreBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::AUTOMATIC_MOVE_TO_ACCOUNT_STORE) {}

MoveToAccountStoreBubbleController::~MoveToAccountStoreBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  if (!interaction_reported_)
    OnBubbleClosing();
}

base::string16 MoveToAccountStoreBubbleController::GetTitle() const {
  return base::string16();
}

void MoveToAccountStoreBubbleController::AcceptMove() {
  return delegate_->MovePasswordToAccountStore();
}

void MoveToAccountStoreBubbleController::ReportInteractions() {}
