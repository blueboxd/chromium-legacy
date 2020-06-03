// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_unsynced_credentials_locally_bubble_controller.h"

#include <utility>

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

SaveUnsyncedCredentialsLocallyBubbleController::
    SaveUnsyncedCredentialsLocallyBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          /*display_disposition=*/metrics_util::
              AUTOMATIC_SAVE_UNSYNCED_CREDENTIALS_LOCALLY),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION) {
}

SaveUnsyncedCredentialsLocallyBubbleController::
    ~SaveUnsyncedCredentialsLocallyBubbleController() {
  if (!interaction_reported_)
    OnBubbleClosing();
}

void SaveUnsyncedCredentialsLocallyBubbleController::OnSaveClicked() {
  delegate_->SaveUnsyncedCredentialsInProfileStore();
}

void SaveUnsyncedCredentialsLocallyBubbleController::OnCancelClicked() {
  delegate_->DiscardUnsyncedCredentials();
}

const std::vector<autofill::PasswordForm>&
SaveUnsyncedCredentialsLocallyBubbleController::GetUnsyncedCredentials() const {
  return delegate_->GetUnsyncedCredentials();
}

void SaveUnsyncedCredentialsLocallyBubbleController::ReportInteractions() {
  metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_)
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
}

base::string16 SaveUnsyncedCredentialsLocallyBubbleController::GetTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UNSYNCED_CREDENTIALS_BUBBLE_TITLE);
}
