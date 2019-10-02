// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/intent_util.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
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
// TODO(crbug.com/853604): Test glob pattern match.
