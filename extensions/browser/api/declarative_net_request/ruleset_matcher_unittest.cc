// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {
namespace {

class RulesetMatcherTest : public ::testing::Test {
 public:
  RulesetMatcherTest() : channel_(::version_info::Channel::UNKNOWN) {}

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcherTest);
};

// Tests a simple blocking rule.
TEST_F(RulesetMatcherTest, BlockingRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_block_request = [&matcher](const RequestParams& params) {
    return !matcher->HasMatchingAllowRule(params) &&
           matcher->HasMatchingBlockRule(params);
  };

  GURL google_url("http://google.com");
  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  EXPECT_TRUE(should_block_request(params));

  GURL yahoo_url("http://yahoo.com");
  params.url = &yahoo_url;
  EXPECT_FALSE(should_block_request(params));
}

// Tests a simple redirect rule.
TEST_F(RulesetMatcherTest, RedirectRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("http://yahoo.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_redirect_request = [&matcher](const RequestParams& params,
                                            GURL* redirect_url) {
    return matcher->GetRedirectRule(params, redirect_url) != nullptr;
  };

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  GURL redirect_url;
  EXPECT_TRUE(should_redirect_request(params, &redirect_url));
  EXPECT_EQ(yahoo_url, redirect_url);

  params.url = &yahoo_url;
  EXPECT_FALSE(should_redirect_request(params, &redirect_url));
}

// Test that a URL cannot redirect to itself, as filed in crbug.com/954646.
TEST_F(RulesetMatcherTest, PreventSelfRedirect) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("go*");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("http://google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL url("http://google.com");
  RequestParams params;
  params.url = &url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  GURL redirect_url;
  EXPECT_FALSE(matcher->GetRedirectRule(params, &redirect_url));
}

// Tests a simple upgrade scheme rule.
TEST_F(RulesetMatcherTest, UpgradeRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("upgradeScheme");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_upgrade_request = [&matcher](const RequestParams& params) {
    return matcher->GetUpgradeRule(params) != nullptr;
  };

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");
  GURL non_upgradeable_url("https://google.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  EXPECT_TRUE(should_upgrade_request(params));

  params.url = &yahoo_url;
  EXPECT_FALSE(should_upgrade_request(params));

  params.url = &non_upgradeable_url;
  EXPECT_FALSE(should_upgrade_request(params));
}

// Tests that a modified ruleset file fails verification.
TEST_F(RulesetMatcherTest, FailedVerification) {
  RulesetSource source = CreateTemporarySource();
  std::unique_ptr<RulesetMatcher> matcher;
  int expected_checksum;
  ASSERT_TRUE(CreateVerifiedMatcher({}, source, &matcher, &expected_checksum));

  // Persist invalid data to the ruleset file and ensure that a version mismatch
  // occurs.
  std::string data = "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(source.indexed_path(), data.c_str(), data.size()));
  EXPECT_EQ(RulesetMatcher::kLoadErrorVersionMismatch,
            RulesetMatcher::CreateVerifiedMatcher(source, expected_checksum,
                                                  &matcher));

  // Now, persist invalid data to the ruleset file, while maintaining the
  // correct version header. Ensure that it fails verification due to checksum
  // mismatch.
  data = GetVersionHeaderForTesting() + "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(source.indexed_path(), data.c_str(), data.size()));
  EXPECT_EQ(RulesetMatcher::kLoadErrorChecksumMismatch,
            RulesetMatcher::CreateVerifiedMatcher(source, expected_checksum,
                                                  &matcher));
}

// Tests IsExtraHeadersMatcher and GetRemoveHeadersMask.
TEST_F(RulesetMatcherTest, RemoveHeaders) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  EXPECT_FALSE(matcher->IsExtraHeadersMatcher());

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;
  EXPECT_EQ(0u, matcher->GetRemoveHeadersMask(params, 0u /* current_mask */));

  rule.action->type = std::string("removeHeaders");
  rule.action->remove_headers_list =
      std::vector<std::string>({"referer", "setCookie"});
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());
  EXPECT_EQ(kRemoveHeadersMask_Referer | kRemoveHeadersMask_SetCookie,
            matcher->GetRemoveHeadersMask(params, 0u /* current_mask */));

  GURL google_url("http://google.com");
  params.url = &google_url;
  EXPECT_EQ(0u, matcher->GetRemoveHeadersMask(params, 0u /* current_mask */));

  // The current mask is ignored while matching and returned as part of the
  // result.
  uint8_t current_mask =
      kRemoveHeadersMask_Referer | kRemoveHeadersMask_SetCookie;
  EXPECT_EQ(current_mask, matcher->GetRemoveHeadersMask(params, current_mask));
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
