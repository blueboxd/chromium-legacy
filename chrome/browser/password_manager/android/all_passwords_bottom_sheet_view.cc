// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_view.h"

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

AllPasswordsBottomSheetView::AllPasswordsBottomSheetView(
    std::unique_ptr<AllPasswordsBottomSheetController> controller)
    : controller_(std::move(controller)) {}

AllPasswordsBottomSheetView::AllPasswordsBottomSheetView(
    AllPasswordsBottomSheetView&&) = default;
AllPasswordsBottomSheetView& AllPasswordsBottomSheetView::operator=(
    AllPasswordsBottomSheetView&&) = default;

AllPasswordsBottomSheetView::~AllPasswordsBottomSheetView() = default;

AllPasswordsBottomSheetView* AllPasswordsBottomSheetView::Create(
    std::unique_ptr<AllPasswordsBottomSheetController> controller) {
  return new AllPasswordsBottomSheetView(std::move(controller));
}

void AllPasswordsBottomSheetView::AllPasswordsBottomSheetView::Show() {
  // TODO(crbug.com/1104132): Implement.
}
