// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/manifest_web_app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"

class ManifestWebAppTest : public InProcessBrowserTest {
 public:
  ManifestWebAppTest() = default;
  ~ManifestWebAppTest() override {}

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  base::HistogramTester* histogram_tester() const {
    return histogram_tester_.get();
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Opens a basic example site in focus mode window.
IN_PROC_BROWSER_TEST_F(ManifestWebAppTest, OpenExampleSite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFocusMode);
  const GURL url("http://example.org/");
  ui_test_utils::NavigateToURL(browser(), url);
  Browser* app_browser = ReparentWebContentsForFocusMode(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  ASSERT_TRUE(app_browser->is_focus_mode());
  ASSERT_FALSE(browser()->is_focus_mode());

  std::unique_ptr<ManifestWebAppBrowserController> controller =
      std::make_unique<ManifestWebAppBrowserController>(app_browser);
  controller->UpdateToolbarVisibility(false);
  // http://example.org is not a secure site, so show toolbar about site
  // information that warn users.
  EXPECT_TRUE(controller->ShouldShowToolbar());
  // Theme color should be default color (white).
  EXPECT_EQ(controller->GetThemeColor(), SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(ManifestWebAppTest, MetricsTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFocusMode);
  Browser* app_browser = ReparentWebContentsForFocusMode(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  const base::TimeDelta duration = base::TimeDelta::FromSeconds(5);
  base::RunLoop run_loop;
  base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();

  CloseBrowserSynchronously(app_browser);
  auto samples =
      histogram_tester()->GetAllSamples("Session.TimeSpentInFocusMode");
  EXPECT_EQ(1u, samples.size());
  EXPECT_LE(duration.InSeconds(), samples.front().min);
}
