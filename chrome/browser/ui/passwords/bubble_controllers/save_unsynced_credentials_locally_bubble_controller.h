// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UNSYNCED_CREDENTIALS_LOCALLY_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UNSYNCED_CREDENTIALS_LOCALLY_BUBBLE_CONTROLLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

class PasswordsModelDelegate;

// This controller provides data and actions for the
// PasswordSaveUnsyncedCredentialsLocallyView.
class SaveUnsyncedCredentialsLocallyBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit SaveUnsyncedCredentialsLocallyBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~SaveUnsyncedCredentialsLocallyBubbleController() override;

  // Called by the view code when the save button is clicked by the user.
  // Saves the uncommitted credentials to the password store.
  void OnSaveClicked();

  // Called by the view code when the cancel button is clicked by the user.
  // Drops the unsynced credentials.
  void OnCancelClicked();

  const std::vector<autofill::PasswordForm>& unsynced_credentials() const {
    return unsynced_credentials_;
  }

 private:
  // PasswordBubbleControllerBase methods:
  base::string16 GetTitle() const override;
  void ReportInteractions() override;

  std::vector<autofill::PasswordForm> unsynced_credentials_;
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UNSYNCED_CREDENTIALS_LOCALLY_BUBBLE_CONTROLLER_H_
