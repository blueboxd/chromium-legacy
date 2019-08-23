// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/previews/core/previews_features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(PreviewsLitePageNavigationThrottleTest, TestGetPreviewsURL) {
  struct TestCase {
    std::string previews_host;
    std::string original_url;
    std::string expected_previews_url;
    std::string experiment_variation;
    std::string experiment_cmd_line;
  };
  const TestCase kTestCases[]{
      // Use https://play.golang.org/p/HUM2HxmUTOW to compute
      // |expected_previews_url|.
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
      },
      {
          "https://previews.host.com",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com:1443",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com:1443/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com:1443",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com:1443/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes#fragment",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "#fragment",
          "",
          "",
      },
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "&x=variation_experiment",
          "variation_experiment",
          "",
      },
      {
          // Ensure that the command line experiment takes precedence over the
          // one provided by variations.
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "&x=cmdline_experiment",
          "variation_experiment",
          "cmdline_experiment",
      },
      {
          "https://previews.host.com",
          "https://[::1]:12345",
          "https://2ikmbopbfxagkb7uer2vgfxmbzu2vw4qq3d3ixe3h2hfhgcabvua."
          "previews.host.com/p?u=https%3A%2F%2F%5B%3A%3A1%5D%3A12345%2F",
          "",
          "",
      },
  };

  for (const TestCase& test_case : kTestCases) {
    variations::testing::ClearAllVariationParams();

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        data_reduction_proxy::switches::kDataReductionProxyExperiment,
        test_case.experiment_cmd_line);

    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> server_experiment_params;
    server_experiment_params[data_reduction_proxy::params::
                                 GetDataSaverServerExperimentsOptionName()] =
        test_case.experiment_variation;

    scoped_feature_list.InitWithFeaturesAndParameters(
        {{data_reduction_proxy::features::kDataReductionProxyServerExperiments,
          {server_experiment_params}},
         {previews::features::kLitePageServerPreviews,
          {{"previews_host", test_case.previews_host}}}},
        {});

    EXPECT_EQ(PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
                  GURL(test_case.original_url)),
              GURL(test_case.expected_previews_url));
  }
}
