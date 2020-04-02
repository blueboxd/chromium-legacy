// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_unsynced_credentials_locally_bubble_controller.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

namespace metrics_util = password_manager::metrics_util;

SaveUnsyncedCredentialsLocallyBubbleController::
    SaveUnsyncedCredentialsLocallyBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          /*display_disposition=*/metrics_util::
              AUTOMATIC_SAVE_UNSYNCED_CREDENTIALS_LOCALLY),
      unsynced_credentials_(delegate_->GetUnsyncedCredentials()),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION) {
  DCHECK(!unsynced_credentials_.empty());
}

SaveUnsyncedCredentialsLocallyBubbleController::
    ~SaveUnsyncedCredentialsLocallyBubbleController() {
  if (!interaction_reported_)
    OnBubbleClosing();
}

void SaveUnsyncedCredentialsLocallyBubbleController::OnSaveClicked() {
  NOTIMPLEMENTED();
}

void SaveUnsyncedCredentialsLocallyBubbleController::OnCancelClicked() {
  // TODO(crbug.com/1060132): This method should clear the unsynced credentials
  // wherever they are being stored.
  NOTIMPLEMENTED();
}

void SaveUnsyncedCredentialsLocallyBubbleController::ReportInteractions() {
  metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_)
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
}

base::string16 SaveUnsyncedCredentialsLocallyBubbleController::GetTitle()
    const {
  // TODO(crbug.com/1062344): Add proper (translated) string.
  return base::ASCIIToUTF16("These passwords were not commited:");
}
