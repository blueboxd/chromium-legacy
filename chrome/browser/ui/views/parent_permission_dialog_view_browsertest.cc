// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/parent_permission_dialog_view.h"

#include <memory>
#include <vector>

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"

// A simple DialogBrowserTest for the ParentPermissionDialogView that just
// shows the dialog.
class ParentPermissionDialogViewBrowserTest : public DialogBrowserTest {
 public:
  ParentPermissionDialogViewBrowserTest() {}

  ParentPermissionDialogViewBrowserTest(
      const ParentPermissionDialogViewBrowserTest&) = delete;
  ParentPermissionDialogViewBrowserTest& operator=(
      const ParentPermissionDialogViewBrowserTest&) = delete;

  void OnParentPermissionPromptDone(
      internal::ParentPermissionDialogViewResult result) {}

  void ShowUi(const std::string& name) override {
    std::vector<base::string16> parent_emails =
        std::vector<base::string16>{base::UTF8ToUTF16("parent1@google.com"),
                                    base::UTF8ToUTF16("parent2@google.com")};

    gfx::ImageSkia icon = extensions::util::GetDefaultExtensionIcon();

    ShowParentPermissionDialog(
        browser()->profile(), browser()->window()->GetNativeWindow(),
        parent_emails, false, icon, base::UTF8ToUTF16("Test Message"), nullptr,
        base::BindOnce(&ParentPermissionDialogViewBrowserTest::
                           OnParentPermissionPromptDone,
                       base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
