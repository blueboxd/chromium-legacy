// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_util.h"

#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kFilterUrl[] = "https://www.google.com/";
}

class IntentUtilTest : public testing::Test {
 protected:
  apps::mojom::ConditionPtr CreateMultiConditionValuesCondition() {
    std::vector<apps::mojom::ConditionValuePtr> condition_values;
    condition_values.push_back(apps_util::MakeConditionValue(
        "https", apps::mojom::PatternMatchType::kNone));
    condition_values.push_back(apps_util::MakeConditionValue(
        "http", apps::mojom::PatternMatchType::kNone));
    auto condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kScheme, std::move(condition_values));
    return condition;
  }

  apps::mojom::IntentFilterPtr CreateIntentFilterForShareTarget(
      const std::string& mime_type) {
    auto intent_filter = apps::mojom::IntentFilter::New();

    apps_util::AddSingleValueCondition(
        apps::mojom::ConditionType::kAction, apps_util::kIntentActionSend,
        apps::mojom::PatternMatchType::kNone, intent_filter);

    apps_util::AddSingleValueCondition(
        apps::mojom::ConditionType::kMimeType, mime_type,
        apps::mojom::PatternMatchType::kMimeType, intent_filter);

    return intent_filter;
  }

  // TODO(crbug.com/1092784): Add other things for a completed intent.
  apps::mojom::IntentPtr CreateShareIntent(const std::string& mime_type) {
    auto intent = apps::mojom::Intent::New();
    intent->action = apps_util::kIntentActionSend;
    intent->mime_type = mime_type;
    return intent;
  }
};

TEST_F(IntentUtilTest, AllConditionMatches) {
  GURL test_url = GURL("https://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, intent_filter));
}

TEST_F(IntentUtilTest, OneConditionDoesnotMatch) {
  GURL test_url = GURL("https://www.abc.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, intent_filter));
}

TEST_F(IntentUtilTest, IntentDoesnotHaveValueToMatch) {
  GURL test_url = GURL("www.abc.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, intent_filter));
}

// Test ConditionMatch with more then one condition values.

TEST_F(IntentUtilTest, OneConditionValueMatch) {
  auto condition = CreateMultiConditionValuesCondition();
  GURL test_url = GURL("https://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  EXPECT_TRUE(apps_util::IntentMatchesCondition(intent, condition));
}

TEST_F(IntentUtilTest, NoneConditionValueMathc) {
  auto condition = CreateMultiConditionValuesCondition();
  GURL test_url = GURL("tel://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  EXPECT_FALSE(apps_util::IntentMatchesCondition(intent, condition));
}

// Test Condition Value match with different pattern match type.
TEST_F(IntentUtilTest, NoneMatchType) {
  auto condition_value = apps_util::MakeConditionValue(
      "https", apps::mojom::PatternMatchType::kNone);
  EXPECT_TRUE(apps_util::ConditionValueMatches("https", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("http", condition_value));
}
TEST_F(IntentUtilTest, LiteralMatchType) {
  auto condition_value = apps_util::MakeConditionValue(
      "https", apps::mojom::PatternMatchType::kLiteral);
  EXPECT_TRUE(apps_util::ConditionValueMatches("https", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("http", condition_value));
}
TEST_F(IntentUtilTest, PrefixMatchType) {
  auto condition_value = apps_util::MakeConditionValue(
      "/ab", apps::mojom::PatternMatchType::kPrefix);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/abc", condition_value));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/ABC", condition_value));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/d", condition_value));
}

TEST_F(IntentUtilTest, GlobMatchType) {
  auto condition_value_star = apps_util::MakeConditionValue(
      "/a*b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/b", condition_value_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/ab", condition_value_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aab", condition_value_star));
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/aaaaaab", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/aabb", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/aabc", condition_value_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/bb", condition_value_star));

  auto condition_value_dot = apps_util::MakeConditionValue(
      "/a.b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aab", condition_value_dot));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/acb", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/ab", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abd", condition_value_dot));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abbd", condition_value_dot));

  auto condition_value_dot_and_star = apps_util::MakeConditionValue(
      "/a.*b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/aab", condition_value_dot_and_star));
  EXPECT_TRUE(apps_util::ConditionValueMatches("/aadsfadslkjb",
                                               condition_value_dot_and_star));
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/ab", condition_value_dot_and_star));

  // This arguably should be true, however the algorithm is transcribed from the
  // upstream Android codebase, which behaves like this.
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abasdfab",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/abasdfad",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(apps_util::ConditionValueMatches("/bbasdfab",
                                                condition_value_dot_and_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/a", condition_value_dot_and_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/b", condition_value_dot_and_star));

  auto condition_value_escape_dot = apps_util::MakeConditionValue(
      "/a\\.b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/a.b", condition_value_escape_dot));

  // This arguably should be false, however the transcribed is carried from the
  // upstream Android codebase, which behaves like this.
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/acb", condition_value_escape_dot));

  auto condition_value_escape_star = apps_util::MakeConditionValue(
      "/a\\*b", apps::mojom::PatternMatchType::kGlob);
  EXPECT_TRUE(
      apps_util::ConditionValueMatches("/a*b", condition_value_escape_star));
  EXPECT_FALSE(
      apps_util::ConditionValueMatches("/acb", condition_value_escape_star));
}

TEST_F(IntentUtilTest, FilterMatchLevel) {
  auto filter_scheme_only = apps_util::CreateSchemeOnlyFilter("http");
  auto filter_scheme_and_host_only =
      apps_util::CreateSchemeAndHostOnlyFilter("https", "www.abc.com");
  auto filter_url = apps_util::CreateIntentFilterForUrlScope(
      GURL("https:://www.google.com/"));
  auto filter_empty = apps::mojom::IntentFilter::New();

  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_url),
            apps_util::IntentFilterMatchLevel::kScheme +
                apps_util::IntentFilterMatchLevel::kHost +
                apps_util::IntentFilterMatchLevel::kPattern);
  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_scheme_and_host_only),
            apps_util::IntentFilterMatchLevel::kScheme +
                apps_util::IntentFilterMatchLevel::kHost);
  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_scheme_only),
            apps_util::IntentFilterMatchLevel::kScheme);
  EXPECT_EQ(apps_util::GetFilterMatchLevel(filter_empty),
            apps_util::IntentFilterMatchLevel::kNone);

  EXPECT_TRUE(apps_util::GetFilterMatchLevel(filter_url) >
              apps_util::GetFilterMatchLevel(filter_scheme_and_host_only));
  EXPECT_TRUE(apps_util::GetFilterMatchLevel(filter_scheme_and_host_only) >
              apps_util::GetFilterMatchLevel(filter_scheme_only));
  EXPECT_TRUE(apps_util::GetFilterMatchLevel(filter_scheme_only) >
              apps_util::GetFilterMatchLevel(filter_empty));
}

TEST_F(IntentUtilTest, ActionMatch) {
  GURL test_url = GURL("https://www.google.com/");
  auto intent = apps_util::CreateIntentFromUrl(test_url);
  auto intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl),
                                               /*with_action_view=*/true);
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent, intent_filter));

  auto send_intent = apps_util::CreateIntentFromUrl(test_url);
  send_intent->action = apps_util::kIntentActionSend;
  EXPECT_FALSE(apps_util::IntentMatchesFilter(send_intent, intent_filter));

  auto send_intent_filter =
      apps_util::CreateIntentFilterForUrlScope(GURL(kFilterUrl),
                                               /*with_action_view=*/true);
  send_intent_filter->conditions[0]->condition_values[0]->value =
      apps_util::kIntentActionSend;
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent, send_intent_filter));
}

TEST_F(IntentUtilTest, MimeTypeMatch) {
  std::string mime_type1 = "text/plain";
  std::string mime_type2 = "image/jpeg";
  std::string mime_type_sub_wildcard = "text/*";
  std::string mime_type_all_wildcard = "*/*";

  auto intent1 = CreateShareIntent(mime_type1);
  auto intent2 = CreateShareIntent(mime_type2);
  auto intent_sub_wildcard = CreateShareIntent(mime_type_sub_wildcard);
  auto intent_all_wildcard = CreateShareIntent(mime_type_all_wildcard);

  auto filter1 = CreateIntentFilterForShareTarget(mime_type1);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter1));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent2, filter1));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent_sub_wildcard, filter1));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent_all_wildcard, filter1));

  auto filter2 = CreateIntentFilterForShareTarget(mime_type2);

  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent1, filter2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent2, filter2));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent_sub_wildcard, filter2));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent_all_wildcard, filter2));

  auto filter_sub_wildcard =
      CreateIntentFilterForShareTarget(mime_type_sub_wildcard);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter_sub_wildcard));
  EXPECT_FALSE(apps_util::IntentMatchesFilter(intent2, filter_sub_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_sub_wildcard, filter_sub_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_all_wildcard, filter_sub_wildcard));

  auto filter_all_wildcard =
      CreateIntentFilterForShareTarget(mime_type_all_wildcard);

  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent1, filter_all_wildcard));
  EXPECT_TRUE(apps_util::IntentMatchesFilter(intent2, filter_all_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_sub_wildcard, filter_all_wildcard));
  EXPECT_TRUE(
      apps_util::IntentMatchesFilter(intent_all_wildcard, filter_all_wildcard));
}
