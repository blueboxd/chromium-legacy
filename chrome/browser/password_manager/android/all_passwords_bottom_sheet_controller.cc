// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_view.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"

AllPasswordsBottomSheetController::AllPasswordsBottomSheetController(
    password_manager::PasswordManagerDriver* driver,
    password_manager::PasswordStore* store)
    : driver_(driver), store_(store) {
  DCHECK(driver_);
  DCHECK(store_);
}

AllPasswordsBottomSheetController::~AllPasswordsBottomSheetController() =
    default;

void AllPasswordsBottomSheetController::Show() {
  store_->GetAllLoginsWithAffiliationAndBrandingInformation(this);
  view_->Show();
  delete view_;
}

AllPasswordsBottomSheetController* AllPasswordsBottomSheetController::Create(
    password_manager::PasswordManagerDriver* driver,
    password_manager::PasswordStore* store) {
  std::unique_ptr<AllPasswordsBottomSheetController> controller =
      std::make_unique<AllPasswordsBottomSheetController>(driver, store);
  AllPasswordsBottomSheetController* raw_controller = controller.get();
  raw_controller->view_ =
      AllPasswordsBottomSheetView::Create(std::move(controller));
  return raw_controller;
}

void AllPasswordsBottomSheetController::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  // TODO(crbug.com/1104132): Implement.
}
