// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {
enum class ContextType {
  kEventPage,
  kServiceWorker,
};

// Returns the newly added WebContents.
// TODO(lazyboy): We have at least 3 versions of this AddTab within
// //extensions, put this in a central place and use that instead.
content::WebContents* AddTab(Browser* browser, const GURL& url) {
  int starting_tab_count = browser->tab_strip_model()->count();
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  int tab_count = browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
  return browser->tab_strip_model()->GetActiveWebContents();
}

}  // namespace

// Tests management API from a non-persistent extension (event page or
// Service Worker).
class ManagementApiNonPersistentApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 protected:
  const Extension* LoadNonPersistentExtension(const char* relative_path) {
    return LoadExtensionWithFlags(test_data_dir_.AppendASCII(relative_path),
                                  GetParam() == ContextType::kEventPage
                                      ? kFlagNone
                                      : kFlagRunAsServiceWorkerBasedExtension);
  }
};

// Tests chrome.management.uninstallSelf API.
IN_PROC_BROWSER_TEST_P(ManagementApiNonPersistentApiTest, UninstallSelf) {
  constexpr char kEventPageBackgroundScript[] = R"({"scripts": ["script.js"]})";
  constexpr char kServiceWorkerBackgroundScript[] =
      R"({"service_worker": "script.js"})";

  constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 2,
           "version": "0.1",
           "background": %s
         })";
  std::string manifest =
      base::StringPrintf(kManifest, GetParam() == ContextType::kEventPage
                                        ? kEventPageBackgroundScript
                                        : kServiceWorkerBackgroundScript);

  // This script uninstalls itself.
  constexpr char kScript[] =
      "chrome.management.uninstallSelf({showConfirmDialog: false});";

  TestExtensionDir test_dir;

  test_dir.WriteManifest(manifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kScript);

  // Construct this before loading the extension, since the extension will
  // immediately uninstall itself when it loads.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));

  base::FilePath path = test_dir.Pack();
  // NOTE: Do not use a scoped_refptr<Extension> as the |extension| might get
  // uninstalled right away.
  const Extension* extension = LoadExtension(path);

  EXPECT_EQ(extension, observer.WaitForExtensionUninstalled());
}

// Tests chrome.management.uninstall with a real user gesture
// (i.e. browserAction.onClicked event).
IN_PROC_BROWSER_TEST_P(ManagementApiNonPersistentApiTest,
                       UninstallViaBrowserAction) {
  const Extension* extension_b = LoadNonPersistentExtension(
      "management/uninstall_via_browser_action/extension_b");
  ASSERT_TRUE(extension_b);
  const ExtensionId extension_b_id = extension_b->id();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  // extension_b should be installed.
  // Note: For the purpose of this test, just checking whether the extension is
  // installed or not (ExtensionRegistry::EVERYTHING)
  // is enough. But for clarity, we check for enabled-ness
  // (ExtensionRegistry::ENABLED) here.
  EXPECT_TRUE(
      registry->GetExtensionById(extension_b_id, ExtensionRegistry::ENABLED));

  // Load extension_a and wait for browserAction.onClicked listener
  // registration.
  ExtensionTestMessageListener listener_added("ready", false);
  const Extension* extension_a = LoadNonPersistentExtension(
      "management/uninstall_via_browser_action/extension_a");
  ASSERT_TRUE(extension_a);
  EXPECT_TRUE(listener_added.WaitUntilSatisfied());

  // We need to accept uninstallation prompt since this is not a
  // self-uninstallation.
  ScopedTestDialogAutoConfirm auto_confirm_uninstall(
      ScopedTestDialogAutoConfirm::ACCEPT);

  ResultCatcher catcher;
  // Click on browser action to start the test, |extension_a| will uninstall
  // |extension_b|.
  {
    content::WebContents* web_contents = AddTab(browser(), GURL("about:blank"));
    ASSERT_TRUE(web_contents);
    ExtensionActionRunner::GetForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->RunAction(extension_a, true);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << message_;

  // extension_b should be gone.
  EXPECT_FALSE(registry->GetExtensionById(extension_b_id,
                                          ExtensionRegistry::EVERYTHING));
}

INSTANTIATE_TEST_SUITE_P(EventPage,
                         ManagementApiNonPersistentApiTest,
                         ::testing::Values(ContextType::kEventPage));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ManagementApiNonPersistentApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

}  // namespace extensions
