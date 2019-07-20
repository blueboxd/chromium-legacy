// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_restricted_directory_dialog_view.h"

#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/resource/resource_bundle.h"

class NativeFileSystemRestrictedDirectoryDialogViewTest
    : public DialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Release builds may strip out unused string resources when
    // enable_resource_whitelist_generation is enabled. Manually override the
    // strings needed by the dialog to ensure they are available for tests.
    // TODO(https://crbug.com/979659): Remove these overrides once the strings
    // are referenced from the Chrome binary.
    auto& shared_resource_bundle = ui::ResourceBundle::GetSharedInstance();
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_TITLE,
        base::ASCIIToUTF16("Can't save to this folder"));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_TEXT,
        base::ASCIIToUTF16("$1 can't save your changes to this folder because "
                           "it contains system files."));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_BUTTON,
        base::ASCIIToUTF16("Choose a different folder"));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    NativeFileSystemRestrictedDirectoryDialogView::ShowDialog(
        kTestOrigin, base::FilePath(FILE_PATH_LITERAL("/foo/bar")),
        base::DoNothing(),
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemRestrictedDirectoryDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
