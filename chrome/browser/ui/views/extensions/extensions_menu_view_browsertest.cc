// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <algorithm>

#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"

class ExtensionsMenuViewBrowserTest : public DialogBrowserTest {
 protected:
  Profile* profile() { return browser()->profile(); }

  void LoadTestExtension(const std::string& extension,
                         bool allow_incognito = false) {
    extensions::ChromeTestExtensionLoader loader(profile());
    loader.set_allow_incognito_access(allow_incognito);
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    extensions_.push_back(
        loader.LoadExtension(test_data_dir.AppendASCII(extension)));
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kExtensionsToolbarMenu);
    DialogBrowserTest::SetUp();
  }

  void SetUpIncognitoBrowser() {
    incognito_browser_ = CreateIncognitoBrowser();
  }

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    views::test::ReduceAnimationDuration(GetExtensionsToolbarContainer());
  }

  void ShowUi(const std::string& name) override {
    ui_test_name_ = name;

    if (name == "ReloadPageBubble") {
      ClickExtensionsMenuButton();
      TriggerSingleExtensionButton();
    } else if (ui_test_name_ == "UninstallDialog_Accept" ||
               ui_test_name_ == "UninstallDialog_Cancel") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();

      LoadTestExtension("extensions/uitest/long_name");
      LoadTestExtension("extensions/uitest/window_open");

      // Without the uninstall dialog the icon should now be invisible.
      EXPECT_FALSE(container->IsActionVisibleOnToolbar(
          container->GetActionForId(extensions_[0]->id())));
      EXPECT_FALSE(container->GetViewForId(extensions_[0]->id())->GetVisible());

      // Trigger uninstall dialog.
      extensions::ExtensionContextMenuModel menu_model(
          extensions_[0].get(), browser(),
          extensions::ExtensionContextMenuModel::VISIBLE, nullptr,
          false /* can_show_icon_in_toolbar */);
      menu_model.ExecuteCommand(
          extensions::ExtensionContextMenuModel::UNINSTALL, 0);

      // Executing UNINSTALL consists of two separate asynchronous processes:
      // - the command itself, which is immediately queued for execution
      // - the animation and display of the uninstall dialog, which is driven by
      //   an animation in the layout
      //
      // Flush the task queue so the first asynchronous process has completed.
      base::RunLoop run_loop;
      base::PostTask(FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();

    } else if (ui_test_name_ == "InstallDialog") {
      LoadTestExtension("extensions/uitest/long_name");
      LoadTestExtension("extensions/uitest/window_open");

      // Trigger post-install dialog.
      ExtensionInstalledBubble::ShowBubble(extensions_[0], browser(),
                                           SkBitmap());
    } else {
      ClickExtensionsMenuButton();
    }

    // Wait for any pending animations to finish so that correct pinned
    // extensions and dialogs are actually showing.
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  }

  bool VerifyUi() override {
    EXPECT_TRUE(DialogBrowserTest::VerifyUi());

    if (ui_test_name_ == "ReloadPageBubble") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      // Clicking the extension should close the extensions menu, pop out the
      // extension, and display the "reload this page" bubble.
      EXPECT_TRUE(container->GetAnchoredWidgetForExtensionForTesting(
          extensions_[0]->id()));
      EXPECT_FALSE(container->GetPoppedOutAction());
      EXPECT_FALSE(ExtensionsMenuView::IsShowing());
    } else if (ui_test_name_ == "UninstallDialog_Accept" ||
               ui_test_name_ == "UninstallDialog_Cancel" ||
               ui_test_name_ == "InstallDialog") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      EXPECT_TRUE(container->IsActionVisibleOnToolbar(
          container->GetActionForId(extensions_[0]->id())));
      EXPECT_TRUE(container->GetViewForId(extensions_[0]->id())->GetVisible());
    }

    return true;
  }

  void DismissUi() override {
    if (ui_test_name_ == "UninstallDialog_Accept" ||
        ui_test_name_ == "UninstallDialog_Cancel") {
      DismissUninstallDialog();
      return;
    }

    if (ui_test_name_ == "InstallDialog") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      views::BubbleDialogDelegateView* const install_bubble =
          container->GetViewForId(extensions_[0]->id())
              ->GetProperty(views::kAnchoredDialogKey);
      ASSERT_TRUE(install_bubble);
      install_bubble->GetWidget()->Close();
      return;
    }

    // Use default implementation for other tests.
    DialogBrowserTest::DismissUi();
  }

  void DismissUninstallDialog() {
    ExtensionsToolbarContainer* const container =
        GetExtensionsToolbarContainer();
    // Accept or cancel the dialog.
    views::BubbleDialogDelegateView* const uninstall_bubble =
        container->GetViewForId(extensions_[0]->id())
            ->GetProperty(views::kAnchoredDialogKey);
    ASSERT_TRUE(uninstall_bubble);
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        uninstall_bubble->GetWidget());
    if (ui_test_name_ == "UninstallDialog_Accept") {
      uninstall_bubble->AcceptDialog();
    } else {
      uninstall_bubble->CancelDialog();
    }
    destroyed_waiter.Wait();

    if (ui_test_name_ == "UninstallDialog_Accept") {
      // Accepting the dialog should remove the item from the container and the
      // ExtensionRegistry.
      EXPECT_EQ(nullptr, container->GetActionForId(extensions_[0]->id()));
      EXPECT_EQ(nullptr, extensions::ExtensionRegistry::Get(profile())
                             ->GetInstalledExtension(extensions_[0]->id()));
    } else {
      // After dismissal the icon should become invisible.
      // Wait for animations to finish.
      views::test::WaitForAnimatingLayoutManager(
          GetExtensionsToolbarContainer());

      // The extension should still be present in the ExtensionRegistry (not
      // uninstalled) when the uninstall dialog is dismissed.
      EXPECT_NE(nullptr, extensions::ExtensionRegistry::Get(profile())
                             ->GetInstalledExtension(extensions_[0]->id()));
      // Without the uninstall dialog present the icon should now be
      // invisible.
      EXPECT_FALSE(container->IsActionVisibleOnToolbar(
          container->GetActionForId(extensions_[0]->id())));
      EXPECT_FALSE(container->GetViewForId(extensions_[0]->id())->GetVisible());
    }
  }

  void ClickExtensionsMenuButton(Browser* browser) {
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->GetExtensionsButton()
        ->OnMousePressed(click_event);
  }

  void ClickExtensionsMenuButton() { ClickExtensionsMenuButton(browser()); }

  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() const {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->extensions_container();
  }

  static std::vector<ExtensionsMenuItemView*> GetExtensionsMenuItemViews() {
    return ExtensionsMenuView::GetExtensionsMenuViewForTesting()
        ->extensions_menu_items_for_testing();
  }

  std::vector<ToolbarActionView*> GetToolbarActionViews() const {
    std::vector<ToolbarActionView*> views;
    for (auto* view : GetExtensionsToolbarContainer()->children()) {
      if (view->GetClassName() == ToolbarActionView::kClassName)
        views.push_back(static_cast<ToolbarActionView*>(view));
    }
    return views;
  }

  std::vector<ToolbarActionView*> GetVisibleToolbarActionViews() const {
    auto views = GetToolbarActionViews();
    base::EraseIf(views, [](views::View* view) { return !view->GetVisible(); });
    return views;
  }

  void TriggerSingleExtensionButton() {
    auto menu_items = GetExtensionsMenuItemViews();
    ASSERT_EQ(1u, menu_items.size());
    ui::MouseEvent click_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                               gfx::Point(), base::TimeTicks(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
    menu_items[0]
        ->primary_action_button_for_testing()
        ->button_controller()
        ->OnMouseReleased(click_event);

    // Wait for animations to finish.
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  }

  std::string ui_test_name_;
  base::test::ScopedFeatureList scoped_feature_list_;
  Browser* incognito_browser_ = nullptr;
  std::vector<scoped_refptr<const extensions::Extension>> extensions_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_default) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_NoExtensions) {
  ShowAndVerifyUi();
}

// Invokes the UI shown when a user has to reload a page in order to run an
// extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvokeUi_ReloadPageBubble) {
  ASSERT_TRUE(embedded_test_server()->Start());
  extensions::TestExtensionDir test_dir;
  // Load an extension that injects scripts at "document_start", which requires
  // reloading the page to inject if permissions are withheld.
  test_dir.WriteManifest(
      R"({
           "name": "Runs Script Everywhere",
           "description": "An extension that runs script everywhere",
           "manifest_version": 2,
           "version": "0.1",
           "content_scripts": [{
             "matches": ["*://*/*"],
             "js": ["script.js"],
             "run_at": "document_start"
           }]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                     "console.log('injected!');");

  extensions_.push_back(
      extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
          test_dir.UnpackedPath()));
  ASSERT_EQ(1u, extensions_.size());
  ASSERT_TRUE(extensions_.front());

  extensions::ScriptingPermissionsModifier(profile(), extensions_.front())
      .SetWithholdHostPermissions(true);

  // Navigate to a page the extension wants to run on.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  {
    content::TestNavigationObserver observer(tab);
    GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, TriggerPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  TriggerSingleExtensionButton();

  // After triggering an extension with a popup, there should a popped-out
  // action and show the view.
  auto visible_icons = GetVisibleToolbarActionViews();
  EXPECT_NE(nullptr, extensions_container->GetPoppedOutAction());
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(extensions_container->GetPoppedOutAction(),
            visible_icons[0]->view_controller());

  extensions_container->HideActivePopup();

  // Wait for animations to finish.
  views::test::WaitForAnimatingLayoutManager(extensions_container);

  // After dismissing the popup there should no longer be a popped-out action
  // and the icon should no longer be visible in the extensions container.
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       TriggeringExtensionClosesMenu) {
  LoadTestExtension("extensions/trigger_actions/browser_action");
  ShowUi("");
  VerifyUi();

  EXPECT_TRUE(ExtensionsMenuView::IsShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      ExtensionsMenuView::GetExtensionsMenuViewForTesting()->GetWidget());
  TriggerSingleExtensionButton();

  destroyed_waiter.Wait();

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();

  // This test should not use a popped-out action, as we want to make sure that
  // the menu closes on its own and not because a popup dialog replaces it.
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());

  EXPECT_FALSE(ExtensionsMenuView::IsShowing());
}

#if defined(OS_WIN)
#define MAYBE_CreatesOneMenuItemPerExtension \
  DISABLED_CreatesOneMenuItemPerExtension
#else
#define MAYBE_CreatesOneMenuItemPerExtension CreatesOneMenuItemPerExtension
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       MAYBE_CreatesOneMenuItemPerExtension) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");
  ShowUi("");
  VerifyUi();
  EXPECT_EQ(2u, extensions_.size());
  EXPECT_EQ(extensions_.size(), GetExtensionsMenuItemViews().size());
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       PinningDisabledInIncognito) {
  LoadTestExtension("extensions/uitest/window_open", true);
  SetUpIncognitoBrowser();

  // Make sure the pinning item is disabled for context menus in the Incognito
  // browser.
  extensions::ExtensionContextMenuModel menu(
      extensions_[0].get(), incognito_browser_,
      extensions::ExtensionContextMenuModel::VISIBLE, nullptr,
      true /* can_show_icon_in_toolbar */);
  EXPECT_FALSE(menu.IsCommandIdEnabled(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY));

  // Show menu and verify that the in-menu pin button is disabled too.
  ClickExtensionsMenuButton(incognito_browser_);

  ASSERT_TRUE(VerifyUi());
  ASSERT_EQ(1u, GetExtensionsMenuItemViews().size());
  EXPECT_EQ(
      views::Button::STATE_DISABLED,
      GetExtensionsMenuItemViews().front()->pin_button_for_testing()->state());

  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ManageExtensionsOpensExtensionsPage) {
  ShowUi("");
  VerifyUi();

  EXPECT_TRUE(ExtensionsMenuView::IsShowing());

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  ExtensionsMenuView::GetExtensionsMenuViewForTesting()
      ->manage_extensions_button_for_testing()
      ->button_controller()
      ->OnMouseReleased(click_event);

  // Clicking the Manage Extensions button should open chrome://extensions.
  EXPECT_EQ(
      chrome::kChromeUIExtensionsURL,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

// Tests that clicking on the context menu button of an extension item opens the
// context menu.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ClickingContextMenuButton) {
  LoadTestExtension("extensions/uitest/window_open");
  ClickExtensionsMenuButton();

  auto menu_items = GetExtensionsMenuItemViews();
  ASSERT_EQ(1u, menu_items.size());
  ExtensionsMenuItemView* item_view = menu_items[0];
  EXPECT_FALSE(item_view->IsContextMenuRunning());

  views::ImageButton* context_menu_button =
      menu_items[0]->context_menu_button_for_testing();
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  context_menu_button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                               gfx::Point(), base::TimeTicks(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  context_menu_button->OnMouseReleased(release_event);

  EXPECT_TRUE(item_view->IsContextMenuRunning());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_InstallDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvokeUi_UninstallDialog_Accept) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvokeUi_UninstallDialog_Cancel) {
  ShowAndVerifyUi();
}

class ActivateWithReloadExtensionsMenuBrowserTest
    : public ExtensionsMenuViewBrowserTest,
      public ::testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(ActivateWithReloadExtensionsMenuBrowserTest,
                       ActivateWithReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadTestExtension("extensions/blocked_actions/content_scripts");
  auto extension = extensions_.back();
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html"));

  ShowUi("");
  VerifyUi();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);

  EXPECT_TRUE(action_runner->WantsToRun(extension.get()));

  TriggerSingleExtensionButton();

  auto* const action_bubble =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container()
          ->GetAnchoredWidgetForExtensionForTesting(extensions_[0]->id())
          ->widget_delegate()
          ->AsDialogDelegate();
  ASSERT_TRUE(action_bubble);

  const bool accept_reload_dialog = GetParam();
  if (accept_reload_dialog) {
    content::TestNavigationObserver observer(web_contents);
    action_bubble->AcceptDialog();
    EXPECT_TRUE(web_contents->IsLoading());
    // Wait for reload to finish.
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    // After reload the extension should be allowed to run.
    EXPECT_FALSE(action_runner->WantsToRun(extension.get()));
  } else {
    action_bubble->CancelDialog();
    EXPECT_FALSE(web_contents->IsLoading());
    EXPECT_TRUE(action_runner->WantsToRun(extension.get()));
  }
}

INSTANTIATE_TEST_SUITE_P(AcceptDialog,
                         ActivateWithReloadExtensionsMenuBrowserTest,
                         testing::Values(true));

INSTANTIATE_TEST_SUITE_P(CancelDialog,
                         ActivateWithReloadExtensionsMenuBrowserTest,
                         testing::Values(false));

// TODO(pbos): Add test coverage that makes sure removing popped-out extensions
// properly disposes of the popup.
