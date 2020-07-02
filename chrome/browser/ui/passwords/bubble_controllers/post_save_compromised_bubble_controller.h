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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class BubbleType {
    // Last compromised password was updated. The user is presumed safe.
    kPasswordUpdatedSafeState = 0,
    // A compromised password was updated and there are more issues to fix.
    kPasswordUpdatedWithMoreToFix = 1,
    // There are stored compromised credentials.
    kUnsafeState = 2,
    kMaxValue = kUnsafeState,
  };
  explicit PostSaveCompromisedBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~PostSaveCompromisedBubbleController() override;

  BubbleType type() const { return type_; }
  base::string16 GetBody() const;
  base::string16 GetButtonText() const;
  int GetImageID(bool dark) const;

  // The user chose to check passwords.
  void OnAccepted();

 private:
  // PasswordBubbleControllerBase:
  base::string16 GetTitle() const override;
  void ReportInteractions() override;

  BubbleType type_;
  bool checked_clicked_ = false;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_POST_SAVE_COMPROMISED_BUBBLE_CONTROLLER_H_
