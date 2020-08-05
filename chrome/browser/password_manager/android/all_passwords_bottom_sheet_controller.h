// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_

#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {
class PasswordManagerDriver;
class PasswordStore;
}  // namespace password_manager

class AllPasswordsBottomSheetView;

// This class gets credentials and creates AllPasswordsBottomSheetView.
class AllPasswordsBottomSheetController
    : public password_manager::PasswordStoreConsumer {
 public:
  AllPasswordsBottomSheetController(
      password_manager::PasswordManagerDriver* driver,
      password_manager::PasswordStore* store);
  ~AllPasswordsBottomSheetController() override;
  AllPasswordsBottomSheetController(const AllPasswordsBottomSheetController&) =
      delete;
  AllPasswordsBottomSheetController& operator=(
      const AllPasswordsBottomSheetController&) = delete;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Initializes the view and the controller.
  // Doesn't take ownership of |driver|.
  // Doesn't take ownership of |store|.
  static AllPasswordsBottomSheetController* Create(
      password_manager::PasswordManagerDriver* driver,
      password_manager::PasswordStore* store);

  // Instructs AllPasswordsBottomSheetView to show the credentials to the user.
  void Show();

 private:
  // The controller doesn't take |driver_| ownership.
  password_manager::PasswordManagerDriver* driver_ = nullptr;

  // The controller doesn't take |store_| ownership.
  password_manager::PasswordStore* store_ = nullptr;

  // The controller takes |view_| ownership.
  AllPasswordsBottomSheetView* view_ = nullptr;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
