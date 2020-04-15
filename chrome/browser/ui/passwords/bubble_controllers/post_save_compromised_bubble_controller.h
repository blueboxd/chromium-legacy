// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_POST_SAVE_COMPROMISED_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_POST_SAVE_COMPROMISED_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

class PasswordsModelDelegate;

// This controller manages the bubble notifying the user about pending
// compromised credentials.
class PostSaveCompromisedBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit PostSaveCompromisedBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~PostSaveCompromisedBubbleController() override;

 private:
  // PasswordBubbleControllerBase:
  base::string16 GetTitle() const override;
  void ReportInteractions() override;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_POST_SAVE_COMPROMISED_BUBBLE_CONTROLLER_H_
