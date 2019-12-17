// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/value_builder.h"

namespace {

scoped_refptr<const extensions::Extension> BuildTestBookmarkApp() {
  extensions::ExtensionBuilder extension_builder("foo");
  extension_builder.AddFlags(extensions::Extension::FROM_BOOKMARK);
  extension_builder.SetManifestPath({"app", "launch", "web_url"},
                                    "https://example.com/");
  return extension_builder.Build();
}

}  // namespace

using BookmarkAppUninstallDialogViewBrowserTest = InProcessBrowserTest;

// Test that WebAppUninstallDialog cancels the uninstall if the Window
// which is passed to WebAppUninstallDialog::Create() is destroyed before
// WebAppUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(BookmarkAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestruction) {
  scoped_refptr<const extensions::Extension> extension(BuildTestBookmarkApp());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog(
      web_app::WebAppUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow()));

  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  bool was_uninstalled = false;
  dialog->ConfirmUninstall(extension->id(),
                           base::BindLambdaForTesting([&](bool uninstalled) {
                             was_uninstalled = uninstalled;
                             run_loop.Quit();
                           }));
  run_loop.Run();
  EXPECT_FALSE(was_uninstalled);
}

// Test that WebAppUninstallDialog cancels the uninstall if the Window
// which is passed to WebAppUninstallDialog::Create() is destroyed after
// WebAppUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(BookmarkAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestructionAfterViewCreation) {
  scoped_refptr<const extensions::Extension> extension(BuildTestBookmarkApp());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog(
      web_app::WebAppUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow()));
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  bool was_uninstalled = false;
  dialog->ConfirmUninstall(extension->id(),
                           base::BindLambdaForTesting([&](bool uninstalled) {
                             was_uninstalled = uninstalled;
                             run_loop.Quit();
                           }));

  // Kill parent window.
  browser()->window()->Close();
  run_loop.Run();
  EXPECT_FALSE(was_uninstalled);
}

#if defined(OS_CHROMEOS)
// Test that we don't crash when uninstalling an extension from a bookmark app
// window in Ash. Context: crbug.com/825554
IN_PROC_BROWSER_TEST_F(BookmarkAppUninstallDialogViewBrowserTest,
                       BookmarkAppWindowAshCrash) {
  scoped_refptr<const extensions::Extension> extension(BuildTestBookmarkApp());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  WebApplicationInfo info;
  info.app_url = GURL("https://test.com/");
  const extensions::Extension* bookmark_app =
      extensions::browsertest_util::InstallBookmarkApp(browser()->profile(),
                                                       info);
  Browser* app_browser = extensions::browsertest_util::LaunchAppBrowser(
      browser()->profile(), bookmark_app);

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog;
  {
    base::RunLoop run_loop;
    dialog = web_app::WebAppUninstallDialog::Create(
        app_browser->profile(), app_browser->window()->GetNativeWindow());
    run_loop.RunUntilIdle();
  }

  {
    base::RunLoop run_loop;
    dialog->ConfirmUninstall(extension->id(), base::DoNothing());
    run_loop.RunUntilIdle();
  }
}
#endif  // defined(OS_CHROMEOS)

class BookmarkAppUninstallDialogViewInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    extension_ = BuildTestBookmarkApp();
    extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service()
        ->AddExtension(extension_.get());

    dialog_ = web_app::WebAppUninstallDialog::Create(
        browser()->profile(), browser()->window()->GetNativeWindow());

    base::RunLoop run_loop;
    dialog_->SetDialogShownCallbackForTesting(run_loop.QuitClosure());

    dialog_->ConfirmUninstall(extension_->id(), base::DoNothing());

    run_loop.Run();
  }

 private:
  void TearDownOnMainThread() override {
    // Dialog holds references to the profile, so it needs to tear down before
    // profiles are deleted.
    dialog_.reset();
  }

  scoped_refptr<const extensions::Extension> extension_;
  std::unique_ptr<web_app::WebAppUninstallDialog> dialog_;
};

IN_PROC_BROWSER_TEST_F(BookmarkAppUninstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ManualUninstall) {
  ShowAndVerifyUi();
}
