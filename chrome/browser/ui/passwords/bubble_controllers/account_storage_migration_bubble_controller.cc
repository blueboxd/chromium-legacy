// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/account_storage_migration_bubble_controller.h"

AccountStorageMigrationBubbleController::
    AccountStorageMigrationBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(std::move(delegate),
                                   password_manager::metrics_util::
                                       AUTOMATIC_ACCOUNT_MIGRATION_PROPOSAL) {}

AccountStorageMigrationBubbleController::
    ~AccountStorageMigrationBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  if (!interaction_reported_)
    OnBubbleClosing();
}

base::string16 AccountStorageMigrationBubbleController::GetTitle() const {
  return base::string16();
}

void AccountStorageMigrationBubbleController::ReportInteractions() {}
