// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chromeos/components/print_management/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrintManagementAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  PrintManagementAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kPrintJobManagementApp}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Print Management App installs and launches correctly. Runs some
// spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(PrintManagementAppIntegrationTest,
                       PrintManagementAppInLauncher) {
  const GURL url(chromeos::kChromeUIPrintManagementAppUrl);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::PRINT_MANAGEMENT, url, "Print Management App"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrintManagementAppIntegrationTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
