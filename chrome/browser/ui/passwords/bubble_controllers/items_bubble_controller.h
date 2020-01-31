// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ITEMS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ITEMS_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"

class PasswordsModelDelegate;

// This controller provides data and actions for the PasswordItemsView.
class ItemsBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit ItemsBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~ItemsBubbleController() override;

  // Called by the view code when the manage button is clicked by the user.
  void OnManageClicked(password_manager::ManagePasswordsReferrer referrer);

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnPasswordAction(const autofill::PasswordForm& password_form,
                        PasswordAction action);

  // Returns the available credentials which match the current site.
  const std::vector<autofill::PasswordForm>& local_credentials() const {
    return local_credentials_;
  }

 private:
  // PasswordBubbleControllerBase methods:
  base::string16 GetTitle() const override;
  void ReportInteractions() override;

  std::vector<autofill::PasswordForm> local_credentials_;
  base::string16 title_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ITEMS_BUBBLE_CONTROLLER_H_
