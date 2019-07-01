// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_permission_view.h"

#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/window/dialog_client_view.h"

class NativeFileSystemPermissionViewTest : public DialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Release builds may strip out unused string resources when
    // enable_resource_whitelist_generation is enabled. Manually override the
    // strings needed by the dialog to ensure they are available for tests.
    // TODO(https://crbug.com/979659): Remove these overrides once the strings
    // are referenced from the Chrome binary.
    auto& shared_resource_bundle = ui::ResourceBundle::GetSharedInstance();
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_FILE_TITLE,
        base::ASCIIToUTF16("Edit file?"));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_DIRECTORY_TITLE,
        base::ASCIIToUTF16("Edit folder?"));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_FILE_TEXT,
        base::ASCIIToUTF16(
            "This will allow $1 to save or edit the following file(s) on your "
            "device. Only do this if you trust the app."));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_DIRECTORY_TEXT,
        base::ASCIIToUTF16(
            "This will allow $1 to make changes to the following folder on "
            "your device. Only do this if you trust the app."));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_ALLOW_TEXT,
        base::ASCIIToUTF16("Allow"));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    widget_ = NativeFileSystemPermissionView::ShowDialog(
        kTestOrigin, base::FilePath(FILE_PATH_LITERAL("/foo/bar")),
        /*is_directory=*/false,
        base::BindLambdaForTesting([&](PermissionAction result) {
          callback_called_ = true;
          callback_result_ = result;
        }),
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));

  views::Widget* widget_ = nullptr;

  bool callback_called_ = false;
  PermissionAction callback_result_ = PermissionAction::IGNORED;
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       AcceptIsntDefaultFocused) {
  ShowUi(std::string());
  EXPECT_NE(widget_->client_view()->AsDialogClientView()->ok_button(),
            widget_->GetFocusManager()->GetFocusedView());
  widget_->Close();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, AcceptRunsCallback) {
  ShowUi(std::string());
  widget_->client_view()->AsDialogClientView()->AcceptWindow();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(PermissionAction::GRANTED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, CancelRunsCallback) {
  ShowUi(std::string());
  widget_->client_view()->AsDialogClientView()->CancelWindow();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(PermissionAction::DISMISSED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, CancelsWhenClosed) {
  ShowUi(std::string());
  widget_->Close();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(PermissionAction::DISMISSED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
