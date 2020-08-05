// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_H_

#include <memory>

class AllPasswordsBottomSheetController;

// Bridge AllPasswordsBottomSheet native and java view code by converting user
// credentials data into java-readable formats.
class AllPasswordsBottomSheetView {
 public:
  AllPasswordsBottomSheetView(
      std::unique_ptr<AllPasswordsBottomSheetController> controller);
  AllPasswordsBottomSheetView(const AllPasswordsBottomSheetView&) = delete;
  AllPasswordsBottomSheetView& operator=(const AllPasswordsBottomSheetView&) =
      delete;
  AllPasswordsBottomSheetView(AllPasswordsBottomSheetView&&);
  AllPasswordsBottomSheetView& operator=(AllPasswordsBottomSheetView&&);
  ~AllPasswordsBottomSheetView();

  // Create an instance of the view with passed controller.
  // return a raw pointer for the view.
  static AllPasswordsBottomSheetView* Create(
      std::unique_ptr<AllPasswordsBottomSheetController> controller);

  // Makes call to java bridge to show AllPasswordsBottomSheet view.
  void Show();

 private:
  std::unique_ptr<AllPasswordsBottomSheetController> controller_ = nullptr;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_H_
