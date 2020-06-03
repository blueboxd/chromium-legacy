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
#include "chrome/browser/search_engines/template_url_service_factory.h"
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
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/test/widget_test.h"

namespace {

// A stub dialog controller that displays the dialog with the supplied params.
class TestDialogController : public SettingsOverriddenDialogController {
 public:
  TestDialogController(ShowParams show_params,
                       base::Optional<DialogResult>* dialog_result_out)
      : show_params_(std::move(show_params)),
        dialog_result_out_(dialog_result_out) {
    DCHECK(dialog_result_out_);
  }
  TestDialogController(const TestDialogController&) = delete;
  TestDialogController& operator=(const TestDialogController&) = delete;
  ~TestDialogController() override = default;

 private:
  bool ShouldShow() override { return true; }
  ShowParams GetShowParams() override { return show_params_; }
  void OnDialogShown() override {}
  void HandleDialogResult(DialogResult result) override {
    ASSERT_FALSE(dialog_result_out_->has_value());
    *dialog_result_out_ = result;
  }

  const ShowParams show_params_;

  // The result to populate. Must outlive this object.
  base::Optional<DialogResult>* const dialog_result_out_;
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
      ShowSimpleDialog(false, browser());
    else if (name == "SimpleDialogWithIcon")
      ShowSimpleDialog(true, browser());
    else if (name == "NtpOverriddenDialog_BackToDefault")
      ShowNtpOverriddenDefaultDialog();
    else if (name == "NtpOverriddenDialog_Generic")
      ShowNtpOverriddenGenericDialog();
    else if (name == "SearchOverriddenDialog")
      ShowSearchOverriddenDialog();
    else
      NOTREACHED() << name;
  }

  // Creates, shows, and returns a dialog anchored to the given |browser|. The
  // dialog is owned by the views framework.
  SettingsOverriddenDialogView* ShowSimpleDialog(bool show_icon,
                                                 Browser* browser) {
    SettingsOverriddenDialogController::ShowParams params{
        base::ASCIIToUTF16("Settings overridden dialog title"),
        base::ASCIIToUTF16(
            "Settings overriden dialog body, which is quite a bit "
            "longer than the title alone")};
    if (show_icon)
      params.icon = &kProductIcon;
    auto* dialog =
        new SettingsOverriddenDialogView(std::make_unique<TestDialogController>(
            std::move(params), &dialog_result_));
    dialog->Show(browser->window()->GetNativeWindow());

    return dialog;
  }

  void ShowNtpOverriddenDefaultDialog() {
    // Load an extension overriding the NTP and open a new tab to trigger the
    // dialog.
    LoadExtensionOverridingNewTab();
    NavigateToNewTab();
  }

  void ShowNtpOverriddenGenericDialog() {
    SetNonDefaultSearchProvider();
    LoadExtensionOverridingNewTab();
    NavigateToNewTab();
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

  base::Optional<SettingsOverriddenDialogController::DialogResult>
  dialog_result() const {
    return dialog_result_;
  }

 private:
  void LoadExtensionOverridingNewTab() {
    base::FilePath test_root_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path));

    Profile* const profile = browser()->profile();
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(profile).LoadExtension(
            test_root_path.AppendASCII("extensions/api_test/override/newtab"));
    ASSERT_TRUE(extension);
  }

  void NavigateToNewTab() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void SetNonDefaultSearchProvider() {
    TemplateURLService* const template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    TemplateURLService::TemplateURLVector template_urls =
        template_url_service->GetTemplateURLs();
    auto iter = std::find_if(template_urls.begin(), template_urls.end(),
                             [template_url_service](const TemplateURL* turl) {
                               // For the test, we can be a bit lazier and just
                               // use HasGoogleBaseURLs() instead of getting the
                               // full search URL.
                               return !turl->HasGoogleBaseURLs(
                                   template_url_service->search_terms_data());
                             });
    ASSERT_TRUE(iter != template_urls.end());

    template_url_service->SetUserSelectedDefaultSearchProvider(*iter);
  }

  std::string test_name_;

  base::Optional<SettingsOverriddenDialogController::DialogResult>
      dialog_result_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

////////////////////////////////////////////////////////////////////////////////
// UI Browser Tests

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_SimpleDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_SimpleDialogWithIcon) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_NtpOverriddenDialog_BackToDefault) {
  // Force the post-install NTP UI to be enabled, so that we can test on all
  // platforms.
  extensions::SetNtpPostInstallUiEnabledForTesting(true);
  ShowAndVerifyUi();
  extensions::SetNtpPostInstallUiEnabledForTesting(false);
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       InvokeUi_NtpOverriddenDialog_Generic) {
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

////////////////////////////////////////////////////////////////////////////////
// Functional Browser Tests

// Verify that if the parent window is closed, the dialog notifies the
// controller that it was closed without any user action.
IN_PROC_BROWSER_TEST_F(SettingsOverriddenDialogViewBrowserTest,
                       DialogWindowClosed) {
  Browser* second_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(second_browser);

  SettingsOverriddenDialogView* dialog =
      ShowSimpleDialog(false, second_browser);

  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      dialog->GetWidget());
  CloseBrowserSynchronously(second_browser);
  widget_destroyed_waiter.Wait();
  ASSERT_TRUE(dialog_result());
  EXPECT_EQ(SettingsOverriddenDialogController::DialogResult::
                kDialogClosedWithoutUserAction,
            *dialog_result());
}
