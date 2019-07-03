// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace {

// Trimmed down version of the class found in geolocation_browsertest.cc.
// Used to observe the creation of a single permission request without
// responding.
class PermissionRequestObserver : public PermissionRequestManager::Observer {
 public:
  explicit PermissionRequestObserver(content::WebContents* web_contents)
      : request_manager_(
            PermissionRequestManager::FromWebContents(web_contents)),
        request_shown_(false) {
    request_manager_->AddObserver(this);
  }
  ~PermissionRequestObserver() override {
    // Safe to remove twice if it happens.
    request_manager_->RemoveObserver(this);
  }

  bool request_shown() { return request_shown_; }

 private:
  // PermissionRequestManager::Observer
  void OnBubbleAdded() override {
    request_shown_ = true;
    request_manager_->RemoveObserver(this);
  }

  PermissionRequestManager* request_manager_;
  bool request_shown_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestObserver);
};

}  // namespace

class WakeLockBrowserTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
};

void WakeLockBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures, "WakeLock");
}

void WakeLockBrowserTest::SetUpOnMainThread() {
  // Navigate to a secure context.
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("localhost", "/simple_page.html"));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_THAT(
      web_contents->GetMainFrame()->GetLastCommittedOrigin().Serialize(),
      testing::StartsWith("http://localhost:"));
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest, RequestPermissionScreen) {
  // Requests for a screen lock should always be granted, and there should be no
  // permission prompt.
  PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::string response;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "WakeLock.requestPermission('screen').then(status => "
      "    domAutomationController.send(status));",
      &response));
  EXPECT_EQ(response, "granted");
  EXPECT_EQ(observer.request_shown(), false);
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest,
                       RequestPermissionScreenNoUserGesture) {
  // Requests for a screen lock should always be granted, and there should be no
  // permission prompt.
  PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::string response;
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "WakeLock.requestPermission('screen').then(status => "
      "    domAutomationController.send(status));",
      &response));
  EXPECT_EQ(response, "granted");
  EXPECT_EQ(observer.request_shown(), false);
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest, RequestPermissionSystem) {
  // Requests for a system lock should always be denied, and there should be no
  // permission prompt.
  PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::string response;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "WakeLock.requestPermission('system').then(status => "
      "    domAutomationController.send(status));",
      &response));
  EXPECT_EQ(response, "denied");
  EXPECT_EQ(observer.request_shown(), false);
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest,
                       RequestPermissionSystemNoUserGesture) {
  // Requests for a system lock should always be denied, and there should be no
  // permission prompt.
  PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::string response;
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "WakeLock.requestPermission('system').then(status => "
      "    domAutomationController.send(status));",
      &response));
  EXPECT_EQ(response, "denied");
  EXPECT_EQ(observer.request_shown(), false);
}
