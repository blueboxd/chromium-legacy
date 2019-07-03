// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif  // defined(OS_CHROMEOS)

namespace extensions {

class InstallManagerBookmarkAppDialogTest : public DialogBrowserTest {
 public:
  InstallManagerBookmarkAppDialogTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsUnifiedInstall}, {});
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& test_suffix) override {
    ASSERT_TRUE(embedded_test_server()->Start());

    std::string page_path;
    if (test_suffix == "FromInstallableSite")
      page_path = "/banners/manifest_test_page.html";
    else if (test_suffix == "FromNonInstallableSite")
      page_path = "/favicon/page_with_favicon.html";
    else
      NOTREACHED();

    AddTabAtIndex(1, GURL(embedded_test_server()->GetURL(page_path)),
                  ui::PAGE_TRANSITION_LINK);

    EXPECT_TRUE(web_app::CanCreateWebApp(browser()));

    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();

    base::RunLoop run_loop;

    web_app::CreateWebAppFromCurrentWebContents(
        browser(), /*force_shortcut_app=*/false,
        base::BindLambdaForTesting(
            [&](const web_app::AppId& app_id, web_app::InstallResultCode code) {
              DCHECK_EQ(web_app::InstallResultCode::kSuccess, code);
              installed_app_id_ = app_id;
              run_loop.Quit();
            }));

    run_loop.Run();
  }

 protected:
  content::WebContents* web_contents() const { return web_contents_; }
  web_app::AppId installed_app_id() const { return installed_app_id_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::WebContents* web_contents_ = nullptr;
  web_app::AppId installed_app_id_;

  DISALLOW_COPY_AND_ASSIGN(InstallManagerBookmarkAppDialogTest);
};

IN_PROC_BROWSER_TEST_F(InstallManagerBookmarkAppDialogTest,
                       CreateWindowedPWA_FromNonInstallableSite) {
  // The chrome::ShowBookmarkAppDialog will be launched because
  // page_with_favicon.html doesn't pass the PWA check.
  chrome::SetAutoAcceptBookmarkAppDialogForTesting(true);
  ShowAndVerifyUi();
  chrome::SetAutoAcceptBookmarkAppDialogForTesting(false);

#if defined(OS_CHROMEOS)
  const ash::ShelfID shelf_id(installed_app_id());
  EXPECT_TRUE(ChromeLauncherController::instance()->IsPinned(shelf_id));
  EXPECT_EQ(
      shelf_id,
      ChromeLauncherController::instance()->shelf_model()->active_shelf_id());
#endif  // defined(OS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(InstallManagerBookmarkAppDialogTest,
                       CreateWindowedPWA_FromInstallableSite) {
  // The chrome::ShowPWAInstallDialog will be launched because
  // manifest_test_page.html passes the PWA check.
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  ShowAndVerifyUi();
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  auto* registry = ExtensionRegistry::Get(browser()->profile());
  const Extension* extension =
      registry->enabled_extensions().GetByID(installed_app_id());
  DCHECK(extension);

  EXPECT_EQ("Manifest test app", extension->name());

  // Ensure the tab is reparented into dedicated app window.
  Browser* app_browser = chrome::FindBrowserWithWebContents(web_contents());
  EXPECT_TRUE(app_browser->is_app());
  EXPECT_NE(app_browser, browser());
}

}  // namespace extensions
