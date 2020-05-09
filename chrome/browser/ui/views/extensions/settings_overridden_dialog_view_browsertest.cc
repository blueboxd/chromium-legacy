// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

namespace {

// A stub dialog controller that displays the dialog with the supplied params.
class TestDialogController : public SettingsOverriddenDialogController {
 public:
  explicit TestDialogController(ShowParams show_params)
      : show_params_(std::move(show_params)) {}
  TestDialogController(const TestDialogController&) = delete;
  TestDialogController& operator=(const TestDialogController&) = delete;
  ~TestDialogController() override = default;

 private:
  bool ShouldShow() override { return true; }
  ShowParams GetShowParams() override { return show_params_; }
  void OnDialogShown() override {}
  void HandleDialogResult(DialogResult result) override {}

  const ShowParams show_params_;
};

}  // namespace

class SettingsOverriddenDialogViewBrowserTest : public DialogBrowserTest {
  void ShowUi(const std::string& name) override {
    ASSERT_EQ("SimpleDialog", name);

    SettingsOverriddenDialogController::ShowParams params{
        base::ASCIIToUTF16("Settings overridden dialog title"),
        base::ASCIIToUTF16(
            "Settings overriden dialog body, which is quite a bit "
            "longer than the title alone")};
    auto* dialog = new SettingsOverriddenDialogView(
        std::make_unique<TestDialogController>(std::move(params)));
    dialog->Show(browser()->window()->GetNativeWindow());
  }
};

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_SimpleDialog) {
  ShowAndVerifyUi();
}
