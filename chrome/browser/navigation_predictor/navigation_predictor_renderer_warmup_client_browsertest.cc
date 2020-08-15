// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const base::Feature kNavigationPredictorRendererWarmup{
    "NavigationPredictorRendererWarmup", base::FEATURE_DISABLED_BY_DEFAULT};
}

// Occasional flakes on Windows (https://crbug.com/1045971).
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

class NavigationPredictorRendererWarmupClientBrowserTest
    : public InProcessBrowserTest {
 public:
  NavigationPredictorRendererWarmupClientBrowserTest() = default;
  ~NavigationPredictorRendererWarmupClientBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        kNavigationPredictorRendererWarmup);
    InProcessBrowserTest::SetUp();
  }

  size_t SpareRendererCount() const {
    size_t count = 0;
    for (content::RenderProcessHost::iterator iter(
             content::RenderProcessHost::AllHostsIterator());
         !iter.IsAtEnd(); iter.Advance()) {
      if (iter.GetCurrentValue()->IsUnused()) {
        count++;
      }
    }
    return count;
  }

  void MakeSpareRenderer() {
    content::RenderProcessHost::WarmupSpareRenderProcessHost(
        browser()->profile());
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void MakeEligibleNavigationPrediction() {
    NavigationPredictorKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OnPredictionUpdated(
            GetWebContents(), GURL("https://www.google.com/search?q=test"),
            NavigationPredictorKeyedService::PredictionSource::
                kAnchorElementsParsedFromWebPage,
            {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorRendererWarmupClientBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(StartsSpareRenderer)) {
  // Navigate to a site so that the default renderer is used.
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/simple.html"));

  MakeEligibleNavigationPrediction();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SpareRendererCount(), 1U);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorRendererWarmupClientBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(PreexistingSpareRenderer)) {
  // Navigate to a site so that the default renderer is used.
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/simple.html"));

  MakeSpareRenderer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SpareRendererCount(), 1U);

  MakeEligibleNavigationPrediction();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SpareRendererCount(), 1U);
}
