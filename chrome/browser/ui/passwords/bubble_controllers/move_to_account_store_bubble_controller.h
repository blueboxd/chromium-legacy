// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

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

 private:
  // PasswordBubbleControllerBase:
  base::string16 GetTitle() const override;
  void ReportInteractions() override;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_
