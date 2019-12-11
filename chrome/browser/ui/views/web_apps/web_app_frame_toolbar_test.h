// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_FRAME_TOOLBAR_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_FRAME_TOOLBAR_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class BrowserNonClientFrameView;
class BrowserView;
class GURL;
class WebAppFrameToolbarView;

class WebAppFrameToolbarTest : public InProcessBrowserTest {
 public:
  WebAppFrameToolbarTest();
  ~WebAppFrameToolbarTest() override;

  void InstallAndLaunchWebApp(const GURL& app_url);

  Browser* app_browser() { return app_browser_; }
  BrowserView* browser_view() { return browser_view_; }
  BrowserNonClientFrameView* frame_view() { return frame_view_; }
  WebAppFrameToolbarView* web_app_frame_toolbar() {
    return web_app_frame_toolbar_;
  }

  // BrowserTestBase:
  void SetUpOnMainThread() override;

 private:
  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  BrowserNonClientFrameView* frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_FRAME_TOOLBAR_TEST_H_
