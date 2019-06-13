// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/window_open_disposition.h"

namespace extensions {
namespace {

// A helper class to track StateStore changes.
class TestStateStoreObserver : public StateStore::TestObserver {
 public:
  TestStateStoreObserver(content::BrowserContext* context,
                         const std::string& extension_id)
      : extension_id_(extension_id), scoped_observer_(this) {
    scoped_observer_.Add(ExtensionSystem::Get(context)->state_store());
  }
  ~TestStateStoreObserver() override {}

  void WillSetExtensionValue(const std::string& extension_id,
                             const std::string& key) override {
    if (extension_id == extension_id_)
      ++updated_values_[key];
  }

  int CountForKey(const std::string& key) const {
    auto iter = updated_values_.find(key);
    return iter == updated_values_.end() ? 0 : iter->second;
  }

 private:
  std::string extension_id_;
  std::map<std::string, int> updated_values_;

  ScopedObserver<StateStore, StateStore::TestObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(TestStateStoreObserver);
};

}  // namespace

class ExtensionActionAPITest : public ExtensionApiTest {
 public:
  ExtensionActionAPITest() {}
  ~ExtensionActionAPITest() override {}

  const char* GetManifestKey(ActionInfo::Type action_type) {
    switch (action_type) {
      case ActionInfo::TYPE_ACTION:
        return manifest_keys::kAction;
      case ActionInfo::TYPE_BROWSER:
        return manifest_keys::kBrowserAction;
      case ActionInfo::TYPE_PAGE:
        return manifest_keys::kPageAction;
    }
    NOTREACHED();
    return nullptr;
  }

  const char* GetAPIName(ActionInfo::Type action_type) {
    switch (action_type) {
      case ActionInfo::TYPE_ACTION:
        return "action";
      case ActionInfo::TYPE_BROWSER:
        return "browserAction";
      case ActionInfo::TYPE_PAGE:
        return "pageAction";
    }
    NOTREACHED();
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionActionAPITest);
};

// Alias these for readability, when a test only exercises one type of action.
using BrowserActionAPITest = ExtensionActionAPITest;
using PageActionAPITest = ExtensionActionAPITest;

// A class that runs tests exercising each type of possible toolbar action.
class MultiActionAPITest
    : public ExtensionActionAPITest,
      public testing::WithParamInterface<ActionInfo::Type> {
 public:
  MultiActionAPITest()
      : current_channel_(
            extension_test_util::GetOverrideChannelForActionType(GetParam())) {}

  // Returns true if the |action| has whatever state its default is on the
  // tab with the given |tab_id|.
  bool ActionHasDefaultState(const ExtensionAction& action, int tab_id) const {
    bool is_visible = action.GetIsVisible(tab_id);
    bool default_is_visible =
        action.default_state() == ActionInfo::STATE_ENABLED;
    return is_visible == default_is_visible;
  }

  // Ensures the |action| is enabled on the tab with the given |tab_id|.
  void EnsureActionIsEnabled(ExtensionAction* action, int tab_id) const {
    if (!action->GetIsVisible(tab_id))
      action->SetIsVisible(tab_id, true);
  }

  // Returns the id of the currently-active tab.
  int GetActiveTabId() const {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return SessionTabHelper::IdForTab(web_contents).id();
  }

  // Returns the action associated with |extension|.
  ExtensionAction* GetExtensionAction(const Extension& extension) {
    auto* action_manager = ExtensionActionManager::Get(profile());
    return action_manager->GetExtensionAction(extension);
  }

 private:
  std::unique_ptr<ScopedCurrentChannel> current_channel_;

  DISALLOW_COPY_AND_ASSIGN(MultiActionAPITest);
};

// Check that updating the browser action badge for a specific tab id does not
// cause a disk write (since we only persist the defaults).
// Only browser actions persist settings.
IN_PROC_BROWSER_TEST_F(BrowserActionAPITest, TestNoUnnecessaryIO) {
  ExtensionTestMessageListener ready_listener("ready", false);

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Extension",
           "description": "An extension",
           "manifest_version": 2,
           "version": "0.1",
           "browser_action": {},
           "background": { "scripts": ["background.js"] }
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "chrome.test.sendMessage('ready');");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // The script template to update the browser action.
  constexpr char kUpdate[] =
      R"(chrome.browserAction.setBadgeText(%s);
         domAutomationController.send('pass');)";
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SessionID tab_id = SessionTabHelper::IdForTab(web_contents);
  constexpr char kBrowserActionKey[] = "browser_action";
  TestStateStoreObserver test_state_store_observer(profile(), extension->id());

  {
    TestExtensionActionAPIObserver test_api_observer(profile(),
                                                     extension->id());
    // First, update a specific tab.
    std::string update_options =
        base::StringPrintf("{text: 'New Text', tabId: %d}", tab_id.id());
    EXPECT_EQ("pass", browsertest_util::ExecuteScriptInBackgroundPage(
                          profile(), extension->id(),
                          base::StringPrintf(kUpdate, update_options.c_str())));
    test_api_observer.Wait();

    // The action update should be associated with the specific tab.
    EXPECT_EQ(web_contents, test_api_observer.last_web_contents());
    // Since this was only updating a specific tab, this should *not* result in
    // a StateStore write. We should only write to the StateStore with new
    // default values.
    EXPECT_EQ(0, test_state_store_observer.CountForKey(kBrowserActionKey));
  }

  {
    TestExtensionActionAPIObserver test_api_observer(profile(),
                                                     extension->id());
    // Next, update the default badge text.
    EXPECT_EQ("pass",
              browsertest_util::ExecuteScriptInBackgroundPage(
                  profile(), extension->id(),
                  base::StringPrintf(kUpdate, "{text: 'Default Text'}")));
    test_api_observer.Wait();
    // The action update should not be associated with a specific tab.
    EXPECT_EQ(nullptr, test_api_observer.last_web_contents());

    // This *should* result in a StateStore write, since we persist the default
    // state of the extension action.
    EXPECT_EQ(1, test_state_store_observer.CountForKey(kBrowserActionKey));
  }
}

// Verify that tab-specific values are cleared on navigation and on tab
// removal. Regression test for https://crbug.com/834033.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest,
                       ValuesAreClearedOnNavigationAndTabRemoval) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Extension",
           "description": "An extension",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {}
         })";

  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), initial_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();

  // There should be no explicit title to start, but should be one if we set
  // one.
  EXPECT_FALSE(action->HasTitle(tab_id));
  action->SetTitle(tab_id, "alpha");
  EXPECT_TRUE(action->HasTitle(tab_id));

  // Navigating should clear the title.
  GURL second_url = embedded_test_server()->GetURL("/title2.html");
  ui_test_utils::NavigateToURL(browser(), second_url);

  EXPECT_EQ(second_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(action->HasTitle(tab_id));

  action->SetTitle(tab_id, "alpha");
  {
    content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
    tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                        TabStripModel::CLOSE_NONE);
    destroyed_watcher.Wait();
  }
  // The title should have been cleared on tab removal as well.
  EXPECT_FALSE(action->HasTitle(tab_id));
}

// Tests that tooltips of an extension action icon can be specified using UTF8.
// See http://crbug.com/25349.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, TitleLocalization) {
  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Hreggvi\u00F0ur is my name",
           "description": "Hreggvi\u00F0ur: l10n action",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {
             "default_title": "Hreggvi\u00F0ur"
           }
         })";

  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur: l10n action"),
            extension->description());
  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur is my name"), extension->name());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();
  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur"), action->GetTitle(tab_id));
  EXPECT_EQ(base::WideToUTF8(L"Hreggvi\u00F0ur"),
            action->GetTitle(ExtensionAction::kDefaultTabId));
}

// Tests dispatching the onClicked event to listeners when the extension action
// in the toolbar is pressed.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, OnClickedDispatching) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Clicking",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {},
           "background": { "scripts": ["background.js"] }
         })";
  constexpr char kBackgroundJsTemplate[] =
      R"(chrome.%s.onClicked.addListener((tab) => {
           // Check a few properties on the tabs object to make sure it's sane.
           chrome.test.assertTrue(!!tab);
           chrome.test.assertTrue(tab.id > 0);
           chrome.test.assertTrue(tab.index > -1);
           chrome.test.notifyPass();
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundJsTemplate, GetAPIName(GetParam())));

  // Though this says "BrowserActionTestUtil", it's actually used for all
  // toolbar actions.
  // TODO(devlin): Rename it to ToolbarActionTestUtil.
  std::unique_ptr<BrowserActionTestUtil> toolbar_helper =
      BrowserActionTestUtil::Create(browser());
  EXPECT_EQ(0, toolbar_helper->NumberOfBrowserActions());
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_EQ(1, toolbar_helper->NumberOfBrowserActions());
  EXPECT_EQ(extension->id(), toolbar_helper->GetExtensionId(0));

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabled(action, tab_id);
  EXPECT_FALSE(action->HasPopup(tab_id));

  ResultCatcher result_catcher;
  toolbar_helper->Press(0);
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests the creation of a popup when one is specified in the manifest.
IN_PROC_BROWSER_TEST_P(MultiActionAPITest, PopupCreation) {
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Clicking",
           "manifest_version": 2,
           "version": "0.1",
           "%s": {
             "default_popup": "popup.html"
           }
         })";

  constexpr char kPopupHtml[] =
      R"(<!doctype html>
         <html>
           <script src="popup.js"></script>
         </html>)";
  constexpr char kPopupJs[] =
      "window.onload = function() { chrome.test.notifyPass(); };";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey(GetParam())));
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"), kPopupJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  std::unique_ptr<BrowserActionTestUtil> toolbar_helper =
      BrowserActionTestUtil::Create(browser());

  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const int tab_id = GetActiveTabId();
  EXPECT_TRUE(ActionHasDefaultState(*action, tab_id));
  EnsureActionIsEnabled(action, tab_id);
  EXPECT_TRUE(action->HasPopup(tab_id));

  ResultCatcher result_catcher;
  toolbar_helper->Press(0);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  ProcessManager* process_manager = ProcessManager::Get(profile());
  ProcessManager::FrameSet frames =
      process_manager->GetRenderFrameHostsForExtension(extension->id());
  ASSERT_EQ(1u, frames.size());
  content::RenderFrameHost* render_frame_host = *frames.begin();
  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            render_frame_host->GetLastCommittedURL());

  content::WebContents* popup_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  ASSERT_TRUE(popup_contents);

  content::WebContentsDestroyedWatcher contents_destroyed(popup_contents);
  EXPECT_TRUE(content::ExecuteScript(popup_contents, "window.close()"));
  contents_destroyed.Wait();

  frames = process_manager->GetRenderFrameHostsForExtension(extension->id());
  EXPECT_EQ(0u, frames.size());
}

INSTANTIATE_TEST_SUITE_P(,
                         MultiActionAPITest,
                         testing::Values(ActionInfo::TYPE_ACTION,
                                         ActionInfo::TYPE_PAGE,
                                         ActionInfo::TYPE_BROWSER));

}  // namespace extensions
