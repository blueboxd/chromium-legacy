// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

using content::WebContents;

class PortalBrowserTest : public InProcessBrowserTest {
 public:
  PortalBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, PortalActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* contents = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1, tab_strip_model->count());

  EXPECT_EQ(true, content::EvalJs(contents, "loadPromise"));
  std::vector<WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  EXPECT_EQ(1u, inner_web_contents.size());
  WebContents* portal_contents = inner_web_contents[0];

  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(portal_contents, tab_strip_model->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       DevToolsWindowStaysOpenAfterActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(contents, "loadPromise"));
  DevToolsWindow* dev_tools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  WebContents* main_web_contents =
      DevToolsWindowTesting::Get(dev_tools_window)->main_web_contents();
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(contents, nullptr));

  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(
                browser()->tab_strip_model()->GetActiveWebContents(), nullptr));
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, HttpBasicAuthenticationInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true,
            content::EvalJs(contents,
                            "new Promise((resolve, reject) => {\n"
                            "  let portal = document.createElement('portal');\n"
                            "  portal.src = '/title2.html';\n"
                            "  portal.onload = () => resolve(true);\n"
                            "  document.body.appendChild(portal);\n"
                            "})"));
  const auto& inner_contents = contents->GetInnerWebContents();
  ASSERT_EQ(inner_contents.size(), 1u);
  WebContents* portal_contents = inner_contents[0];
  content::NavigationController& portal_controller =
      portal_contents->GetController();

  LoginPromptBrowserTestObserver login_observer;
  login_observer.Register(
      content::Source<content::NavigationController>(&portal_controller));
  WindowedAuthNeededObserver auth_needed(&portal_controller);
  ASSERT_TRUE(content::ExecJs(portal_contents,
                              "location.href = '/auth-basic?realm=Aperture'"));
  auth_needed.Wait();

  WindowedAuthSuppliedObserver auth_supplied(&portal_controller);
  LoginHandler* login_handler = login_observer.handlers().front();
  EXPECT_EQ(login_handler->auth_info().realm, "Aperture");
  login_handler->SetAuth(base::ASCIIToUTF16("basicuser"),
                         base::ASCIIToUTF16("secret"));
  auth_supplied.Wait();

  base::string16 expected_title = base::ASCIIToUTF16("basicuser/secret");
  content::TitleWatcher title_watcher(portal_contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}
