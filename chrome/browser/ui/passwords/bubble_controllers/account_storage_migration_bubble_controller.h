// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ACCOUNT_STORAGE_MIGRATION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ACCOUNT_STORAGE_MIGRATION_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

class PasswordsModelDelegate;

// This controller manages the bubble asking the user to move a local credential
// to the account storage.
class AccountStorageMigrationBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit AccountStorageMigrationBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~AccountStorageMigrationBubbleController() override;

 private:
  // PasswordBubbleControllerBase:
  base::string16 GetTitle() const override;
  void ReportInteractions() override;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ACCOUNT_STORAGE_MIGRATION_BUBBLE_CONTROLLER_H_
