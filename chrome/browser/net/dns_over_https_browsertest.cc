// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/dns_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct DohParameter {
  std::string doh_provider;
  std::string doh_template;
  bool is_valid;
};

std::vector<DohParameter> GetDohServerTestCases() {
  std::vector<DohParameter> doh_test_cases;
  const auto& doh_server_templates = net::GetDohServerTemplatesListForTesting();
  for (const auto& server_template : doh_server_templates) {
    doh_test_cases.push_back(
        DohParameter({server_template.first, server_template.second, true}));
  }
  // Negative test-case
  doh_test_cases.push_back(DohParameter(
      {"NegativeTestExampleCom", "https://www.example.com", false}));
  return doh_test_cases;
}

}  // namespace

class DohBrowserTest : public InProcessBrowserTest,
                       public testing::WithParamInterface<DohParameter> {
 public:
  DohBrowserTest() : test_url_("https://www.google.com") {
    // Allow test to use full host resolver code, instead of the test resolver
    set_allow_network_access_to_host_resolutions();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {// {features::kNetworkServiceInProcess, {}}, // Turn on for debugging
         {features::kDnsOverHttps,
          {{"Fallback", "false"}, {"Templates", GetParam().doh_template}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const GURL test_url_;
};

IN_PROC_BROWSER_TEST_P(DohBrowserTest, MANUAL_ExternalDohServers) {
  content::TestNavigationObserver nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url_));
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(GetParam().is_valid, nav_observer.last_navigation_succeeded());
}

INSTANTIATE_TEST_SUITE_P(
    DohBrowserParameterizedTest,
    DohBrowserTest,
    ::testing::ValuesIn(GetDohServerTestCases()),
    [](const testing::TestParamInfo<DohBrowserTest::ParamType>& info) {
      return info.param.doh_provider;
    });
