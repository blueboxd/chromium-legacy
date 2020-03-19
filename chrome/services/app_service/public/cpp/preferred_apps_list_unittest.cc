// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/preferred_apps_list.h"

#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/cpp/intent_test_util.h"
#include "chrome/services/app_service/public/cpp/intent_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAppId1[] = "abcdefg";
const char kAppId2[] = "gfedcba";
const char kAppId3[] = "hahahahaha";

}  // namespace

class PreferredAppListTest : public testing::Test {
 protected:
  apps::mojom::IntentFilterPtr CreatePatternFilter(
      const std::string& pattern,
      apps::mojom::PatternMatchType match_type) {
    auto intent_filter =
        apps_util::CreateSchemeAndHostOnlyFilter("https", "www.google.com");
    auto pattern_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kPattern,
                                 std::vector<apps::mojom::ConditionValuePtr>());
    intent_filter->conditions.push_back(std::move(pattern_condition));
    auto condition_value = apps_util::MakeConditionValue(pattern, match_type);
    intent_filter->conditions[2]->condition_values.push_back(
        std::move(condition_value));
    return intent_filter;
  }

  apps::PreferredAppsList preferred_apps_;
};

// Test that for a single preferred app with URL filter, we can add
// and find (or not find) the correct preferred app id for different
// URLs.
TEST_F(PreferredAppListTest, AddPreferredAppForURL) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  GURL url_in_scope = GURL("https://www.google.com/abcde");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_wrong_scheme = GURL("tel://www.google.com/");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));

  GURL url_wrong_host = GURL("https://www.hahaha.com/");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_host));

  GURL url_not_in_scope = GURL("https://www.google.com/a");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for preferred app with filter that does not have all condition
// types. E.g. add preferred app with intent filter that only have scheme.
TEST_F(PreferredAppListTest, TopLayerFilters) {
  auto intent_filter = apps_util::CreateSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_in_scope = GURL("tel://1234556/");
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_in_scope));

  GURL url_not_in_scope = GURL("http://www.google.com");
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_not_in_scope));
}

// Test for multiple preferred app setting with different number of condition
// types.
TEST_F(PreferredAppListTest, MixLayerFilters) {
  auto intent_filter_scheme = apps_util::CreateSchemeOnlyFilter("tel");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_scheme);

  auto intent_filter_scheme_host =
      apps_util::CreateSchemeAndHostOnlyFilter("http", "www.abc.com");
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_scheme_host);

  auto intent_filter_url =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_url);

  GURL url_1 = GURL("tel://1234556/");
  GURL url_2 = GURL("http://www.abc.com/");
  GURL url_3 = GURL("https://www.google.com/");
  GURL url_out_scope = GURL("https://www.abc.com/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that when there are multiple preferred apps for one intent, the best
// matching one will be picked.
TEST_F(PreferredAppListTest, MultiplePreferredApps) {
  GURL url = GURL("https://www.google.com/");

  auto intent_filter_scheme = apps_util::CreateSchemeOnlyFilter("https");
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_scheme);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url));

  auto intent_filter_scheme_host =
      apps_util::CreateSchemeAndHostOnlyFilter("https", "www.google.com");
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_scheme_host);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url));

  auto intent_filter_url =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_url);

  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url));
}

// Test that we can properly add and search for filters that has multiple
// condition values for a condition type.
TEST_F(PreferredAppListTest, MultipleConditionValues) {
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.google.com/"));
  intent_filter->conditions[0]->condition_values.push_back(
      apps_util::MakeConditionValue("http",
                                    apps::mojom::PatternMatchType::kNone));

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  GURL url_https = GURL("https://www.google.com/");
  GURL url_http = GURL("http://www.google.com/");
  GURL url_http_out_of_scope = GURL("http://www.abc.com/");
  GURL url_wrong_scheme = GURL("tel://1234567/");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_https));
  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_http));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_http_out_of_scope));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_wrong_scheme));
}

// Test for more than one pattern available, we can find the correct match.
TEST_F(PreferredAppListTest, DifferentPatterns) {
  auto intent_filter_literal =
      CreatePatternFilter("/bc", apps::mojom::PatternMatchType::kLiteral);
  auto intent_filter_prefix =
      CreatePatternFilter("/a", apps::mojom::PatternMatchType::kPrefix);
  auto intent_filter_glob =
      CreatePatternFilter("/c.*d", apps::mojom::PatternMatchType::kGlob);

  preferred_apps_.AddPreferredApp(kAppId1, intent_filter_literal);
  preferred_apps_.AddPreferredApp(kAppId2, intent_filter_prefix);
  preferred_apps_.AddPreferredApp(kAppId3, intent_filter_glob);

  GURL url_1 = GURL("https://www.google.com/bc");
  GURL url_2 = GURL("https://www.google.com/abbb");
  GURL url_3 = GURL("https://www.google.com/ccccccd");
  GURL url_out_scope = GURL("https://www.google.com/dfg");

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(url_1));
  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(url_2));
  EXPECT_EQ(kAppId3, preferred_apps_.FindPreferredAppForUrl(url_3));
  EXPECT_EQ(base::nullopt,
            preferred_apps_.FindPreferredAppForUrl(url_out_scope));
}

// Test that for same intent filter, the app id will overwrite the old setting.
TEST_F(PreferredAppListTest, OverwritePreferredApp) {
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  preferred_apps_.AddPreferredApp(kAppId1, intent_filter);

  EXPECT_EQ(kAppId1, preferred_apps_.FindPreferredAppForUrl(filter_url));

  preferred_apps_.AddPreferredApp(kAppId2, intent_filter);

  EXPECT_EQ(kAppId2, preferred_apps_.FindPreferredAppForUrl(filter_url));
}
