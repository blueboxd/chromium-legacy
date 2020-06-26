// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/gfx/image/image.h"

class PasswordsModelDelegate;

// This controller manages the bubble asking the user to move a profile
// credential to their account store.
class MoveToAccountStoreBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit MoveToAccountStoreBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~MoveToAccountStoreBubbleController() override;

  // Called by the view when the user clicks the confirmation button.
  void AcceptMove();

  // Called by the view when the user clicks the "No, thanks" button.
  void RejectMove();

  // Returns either a large site icon or a fallback icon.
  gfx::Image GetProfileIcon();

 private:
  // PasswordBubbleControllerBase:
  base::string16 GetTitle() const override;
  void ReportInteractions() override;

  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_
