// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view.h"

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

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
 public:
  SettingsOverriddenDialogViewBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExtensionSettingsOverriddenDialogs);
  }
  ~SettingsOverriddenDialogViewBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    test_name_ = name;
    if (name == "SimpleDialog")
      ShowSimpleDialog(false);
    else if (name == "SimpleDialogWithIcon")
      ShowSimpleDialog(true);
    else if (name == "NtpOverriddenDialog")
      ShowNtpOverriddenDialog();
    else if (name == "SearchOverriddenDialog")
      ShowSearchOverriddenDialog();
    else
      NOTREACHED() << name;
  }

  void ShowSimpleDialog(bool show_icon) {
    SettingsOverriddenDialogController::ShowParams params{
        base::ASCIIToUTF16("Settings overridden dialog title"),
        base::ASCIIToUTF16(
            "Settings overriden dialog body, which is quite a bit "
            "longer than the title alone")};
    if (show_icon)
      params.icon = &kProductIcon;
    auto* dialog = new SettingsOverriddenDialogView(
        std::make_unique<TestDialogController>(std::move(params)));
    dialog->Show(browser()->window()->GetNativeWindow());
  }

  void ShowNtpOverriddenDialog() {
    base::FilePath test_root_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path));

    // Load up an extension that overrides the NTP.
    Profile* const profile = browser()->profile();
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(profile).LoadExtension(
            test_root_path.AppendASCII("extensions/api_test/override/newtab"));
    ASSERT_TRUE(extension);

    // Opening up a new tab should trigger the dialog.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void ShowSearchOverriddenDialog() {
    base::FilePath test_root_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path));

    // Load up an extension that overrides search.
    Profile* const profile = browser()->profile();
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(profile).LoadExtension(
            test_root_path.AppendASCII("extensions/search_provider_override"));
    ASSERT_TRUE(extension);

    // Perform a search via the omnibox to trigger the dialog.
    ui_test_utils::SendToOmniboxAndSubmit(browser(), "Penguin",
                                          base::TimeTicks::Now());
    content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi())
      return false;

    if (test_name_ == "SearchOverriddenDialog") {
      // Note: Because this is a test, we don't actually expect this navigation
      // to succeed. But we can still check that the user was sent to
      // example.com (the new search engine).
      EXPECT_EQ("www.example.com", browser()
                                       ->tab_strip_model()
                                       ->GetActiveWebContents()
                                       ->GetLastCommittedURL()
                                       .host_piece());
    }

    return true;
  }

 private:
  std::string test_name_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_SimpleDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_SimpleDialogWithIcon) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_NtpOverriddenDialog) {
  // Force the post-install NTP UI to be enabled, so that we can test on all
  // platforms.
  extensions::SetNtpPostInstallUiEnabledForTesting(true);
  ShowAndVerifyUi();
  extensions::SetNtpPostInstallUiEnabledForTesting(false);
}

// The chrome_settings_overrides API that allows extensions to override the
// default search provider is only available on Windows and Mac.
#if defined(OS_WIN) || defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_SearchOverriddenDialog) {
  ShowAndVerifyUi();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
