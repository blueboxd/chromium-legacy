// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

class NearbyShareDialogUITest : public InProcessBrowserTest {
 public:
  NearbyShareDialogUITest() {
    scoped_feature_list_.InitWithFeatures({features::kNearbySharing}, {});
  }
  ~NearbyShareDialogUITest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NearbyShareDialogUITest, RendersComponent) {
  // First, check that navigation succeeds.
  GURL kUrl(content::GetWebUIURL(chrome::kChromeUINearbyShareHost));
  ui_test_utils::NavigateToURL(browser(), kUrl);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(kUrl, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(web_contents->IsCrashed());

  // Assert that we render the nearby-share-app component.
  int num_nearby_share_app = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "domAutomationController.send("
      "document.getElementsByTagName('nearby-share-app').length)",
      &num_nearby_share_app));
  EXPECT_EQ(1, num_nearby_share_app);
}
