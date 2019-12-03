// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"

class HelpAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  HelpAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kHelpAppV2}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Help App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_F(HelpAppIntegrationTest, HelpAppV2) {
  const GURL url(chromeos::kChromeUIHelpAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::HELP, url, "Help App"));
}
