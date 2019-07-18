// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <string>
#include <utility>
#include <vector>

#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

using PageAccess = PermissionsData::PageAccess;
using RedirectAction = CompositeMatcher::RedirectAction;

class CompositeMatcherTest : public ::testing::Test {
 public:
  CompositeMatcherTest() : channel_(::version_info::Channel::UNKNOWN) {}

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcherTest);
};

// Ensure CompositeMatcher respects priority of individual rulesets.
TEST_F(CompositeMatcherTest, RulesetPriority) {
  TestRule block_rule = CreateGenericRule();
  block_rule.condition->url_filter = std::string("google.com");
  block_rule.id = kMinValidID;

  TestRule redirect_rule_1 = CreateGenericRule();
  redirect_rule_1.condition->url_filter = std::string("example.com");
  redirect_rule_1.priority = kMinValidPriority;
  redirect_rule_1.action->type = std::string("redirect");
  redirect_rule_1.action->redirect_url = std::string("http://ruleset1.com");
  redirect_rule_1.id = kMinValidID + 1;

  // Create the first ruleset matcher.
  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule, redirect_rule_1},
      CreateTemporarySource(kSource1ID, kSource1Priority), &matcher_1));

  // Now create a second ruleset matcher.
  const size_t kSource2ID = 2;
  const size_t kSource2Priority = 2;
  TestRule allow_rule = block_rule;
  allow_rule.action->type = std::string("allow");
  TestRule redirect_rule_2 = redirect_rule_1;
  redirect_rule_2.action->redirect_url = std::string("http://ruleset2.com");
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule, redirect_rule_2},
      CreateTemporarySource(kSource2ID, kSource2Priority), &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  GURL google_url = GURL("http://google.com");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // The second ruleset should get more priority.
  EXPECT_FALSE(composite_matcher->ShouldBlockRequest(google_params));

  GURL example_url = GURL("http://example.com");
  RequestParams example_params;
  example_params.url = &example_url;
  example_params.element_type =
      url_pattern_index::flat::ElementType_SUBDOCUMENT;
  example_params.is_third_party = false;

  RedirectAction action = composite_matcher->ShouldRedirectRequest(
      example_params, PageAccess::kAllowed);
  EXPECT_EQ(GURL("http://ruleset2.com"), action.redirect_url);
  EXPECT_FALSE(action.notify_request_withheld);

  // Now switch the priority of the two rulesets. This requires re-constructing
  // the two ruleset matchers.
  matcher_1.reset();
  matcher_2.reset();
  matchers.clear();
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule, redirect_rule_1},
      CreateTemporarySource(kSource1ID, kSource2Priority), &matcher_1));
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule, redirect_rule_2},
      CreateTemporarySource(kSource2ID, kSource1Priority), &matcher_2));
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  composite_matcher = std::make_unique<CompositeMatcher>(std::move(matchers));

  // Reusing request params means that their allow_rule_caches must be cleared.
  google_params.allow_rule_cache.clear();
  example_params.allow_rule_cache.clear();

  // The first ruleset should get more priority.
  EXPECT_TRUE(composite_matcher->ShouldBlockRequest(google_params));

  action = composite_matcher->ShouldRedirectRequest(example_params,
                                                    PageAccess::kAllowed);
  EXPECT_EQ(GURL("http://ruleset1.com"), action.redirect_url);
  EXPECT_FALSE(action.notify_request_withheld);
}

// Ensure allow rules in a higher priority matcher override redirect
// and removeHeader rules from lower priority matchers.
TEST_F(CompositeMatcherTest, AllowRuleOverrides) {
  TestRule allow_rule_1 = CreateGenericRule();
  allow_rule_1.id = kMinValidID;
  allow_rule_1.condition->url_filter = std::string("google.com");
  allow_rule_1.action->type = std::string("allow");

  TestRule remove_headers_rule_1 = CreateGenericRule();
  remove_headers_rule_1.id = kMinValidID + 1;
  remove_headers_rule_1.condition->url_filter = std::string("example.com");
  remove_headers_rule_1.action->type = std::string("removeHeaders");
  remove_headers_rule_1.action->remove_headers_list =
      std::vector<std::string>({"referer", "setCookie"});

  // Create the first ruleset matcher, which allows requests to google.com and
  // removes headers from requests to example.com.
  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule_1, remove_headers_rule_1},
      CreateTemporarySource(kSource1ID, kSource1Priority), &matcher_1));

  // Now set up rules and the second matcher.
  TestRule allow_rule_2 = allow_rule_1;
  allow_rule_2.condition->url_filter = std::string("example.com");

  TestRule redirect_rule_2 = CreateGenericRule();
  redirect_rule_2.condition->url_filter = std::string("google.com");
  redirect_rule_2.priority = kMinValidPriority;
  redirect_rule_2.action->type = std::string("redirect");
  redirect_rule_2.action->redirect_url = std::string("http://ruleset2.com");
  redirect_rule_2.id = kMinValidID + 1;

  // Create a second ruleset matcher, which allows requests to example.com and
  // redirects requests to google.com.
  const size_t kSource2ID = 2;
  const size_t kSource2Priority = 2;
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule_2, redirect_rule_2},
      CreateTemporarySource(kSource2ID, kSource2Priority), &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  // Send a request to google.com which should be redirected.
  GURL google_url = GURL("http://google.com");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // The second ruleset should get more priority.
  RedirectAction action = composite_matcher->ShouldRedirectRequest(
      google_params, PageAccess::kAllowed);
  EXPECT_EQ(GURL("http://ruleset2.com"), action.redirect_url);
  EXPECT_FALSE(action.notify_request_withheld);

  // Send a request to example.com with headers, expect the allow rule to be
  // matched and the headers to remain.
  GURL example_url = GURL("http://example.com");
  RequestParams example_params;
  example_params.url = &example_url;
  example_params.element_type =
      url_pattern_index::flat::ElementType_SUBDOCUMENT;
  example_params.is_third_party = false;

  // Expect no headers to be removed.
  EXPECT_EQ(0u, composite_matcher->GetRemoveHeadersMask(example_params, 0u));

  // Now switch the priority of the two rulesets. This requires re-constructing
  // the two ruleset matchers.
  matcher_1.reset();
  matcher_2.reset();
  matchers.clear();
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule_1, remove_headers_rule_1},
      CreateTemporarySource(kSource1ID, kSource2Priority), &matcher_1));
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule_2, redirect_rule_2},
      CreateTemporarySource(kSource2ID, kSource1Priority), &matcher_2));
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  composite_matcher = std::make_unique<CompositeMatcher>(std::move(matchers));

  // Reusing request params means that their allow_rule_caches must be cleared.
  google_params.allow_rule_cache.clear();
  example_params.allow_rule_cache.clear();

  // The first ruleset should get more priority and so the request to google.com
  // should not be redirected.
  action = composite_matcher->ShouldRedirectRequest(google_params,
                                                    PageAccess::kAllowed);
  EXPECT_FALSE(action.redirect_url);
  EXPECT_FALSE(action.notify_request_withheld);

  // The request to example.com should now have its headers removed.
  example_params.allow_rule_cache.clear();
  uint8_t expected_mask =
      kRemoveHeadersMask_Referer | kRemoveHeadersMask_SetCookie;
  EXPECT_EQ(expected_mask,
            composite_matcher->GetRemoveHeadersMask(example_params, 0u));
}

// Ensure CompositeMatcher detects requests to be notified based on the rule
// matched and whether the extenion has access to the request.
TEST_F(CompositeMatcherTest, NotifyWithholdFromPageAccess) {
  TestRule redirect_rule = CreateGenericRule();
  redirect_rule.condition->url_filter = std::string("google.com");
  redirect_rule.priority = kMinValidPriority;
  redirect_rule.action->type = std::string("redirect");
  redirect_rule.action->redirect_url = std::string("http://ruleset1.com");
  redirect_rule.id = kMinValidID;

  TestRule upgrade_rule = CreateGenericRule();
  upgrade_rule.condition->url_filter = std::string("example.com");
  upgrade_rule.priority = kMinValidPriority + 1;
  upgrade_rule.action->type = std::string("upgradeScheme");
  upgrade_rule.id = kMinValidID + 1;

  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {redirect_rule, upgrade_rule},
      CreateTemporarySource(kSource1ID, kSource1Priority), &matcher_1));

  // Create a composite matcher.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  GURL google_url = GURL("http://google.com");
  GURL example_url = GURL("http://example.com");
  GURL yahoo_url = GURL("http://yahoo.com");

  GURL ruleset1_url = GURL("http://ruleset1.com");
  GURL https_example_url = GURL("https://example.com");

  struct {
    GURL& request_url;
    PageAccess access;
    base::Optional<GURL> expected_final_url;
    bool should_notify_withheld;
  } test_cases[] = {
      // If access to the request is allowed, we should not notify that
      // the request is withheld.
      {google_url, PageAccess::kAllowed, ruleset1_url, false},
      {example_url, PageAccess::kAllowed, https_example_url, false},
      {yahoo_url, PageAccess::kAllowed, base::nullopt, false},

      // Notify the request is withheld if it matches with a redirect rule.
      {google_url, PageAccess::kWithheld, base::nullopt, true},
      // If the page access to the request is withheld but it matches with
      // an upgrade rule, or no rule, then we should not notify.
      {example_url, PageAccess::kWithheld, https_example_url, false},
      {yahoo_url, PageAccess::kWithheld, base::nullopt, false},

      // If access to the request is denied instead of withheld, the extension
      // should not be notified.
      {google_url, PageAccess::kDenied, base::nullopt, false},
      // If the page access to the request is denied but it matches with
      // an upgrade rule, or no rule, then we should not notify.
      {example_url, PageAccess::kDenied, https_example_url, false},
      {yahoo_url, PageAccess::kDenied, base::nullopt, false},
  };

  for (const auto& test_case : test_cases) {
    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    RedirectAction redirect_action =
        composite_matcher->ShouldRedirectRequest(params, test_case.access);

    EXPECT_EQ(test_case.should_notify_withheld,
              redirect_action.notify_request_withheld);
  }
}

// Tests that the redirect url within an extension's ruleset is chosen based on
// the highest priority matching rule.
TEST_F(CompositeMatcherTest, GetRedirectUrlFromPriority) {
  TestRule abc_redirect = CreateGenericRule();
  abc_redirect.condition->url_filter = std::string("*abc*");
  abc_redirect.priority = kMinValidPriority;
  abc_redirect.action->type = std::string("redirect");
  abc_redirect.action->redirect_url = std::string("http://google.com");
  abc_redirect.id = kMinValidID;

  TestRule def_upgrade = CreateGenericRule();
  def_upgrade.condition->url_filter = std::string("*def*");
  def_upgrade.priority = kMinValidPriority + 1;
  def_upgrade.action->type = std::string("upgradeScheme");
  def_upgrade.id = kMinValidID + 1;

  TestRule ghi_redirect = CreateGenericRule();
  ghi_redirect.condition->url_filter = std::string("*ghi*");
  ghi_redirect.priority = kMinValidPriority + 2;
  ghi_redirect.action->type = std::string("redirect");
  ghi_redirect.action->redirect_url = std::string("http://example.com");
  ghi_redirect.id = kMinValidID + 2;

  // In terms of priority: ghi > def > abc.

  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {abc_redirect, def_upgrade, ghi_redirect},
      CreateTemporarySource(kSource1ID, kSource1Priority), &matcher_1));

  // Create a composite matcher.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  struct {
    GURL request_url;
    base::Optional<GURL> expected_final_url;
  } test_cases[] = {
      // Test requests which match exactly one rule.
      {GURL("http://abc.com"), GURL("http://google.com")},
      {GURL("http://def.com"), GURL("https://def.com")},
      {GURL("http://ghi.com"), GURL("http://example.com")},

      // The upgrade rule has a higher priority than the redirect rule matched
      // so the request should be upgraded.
      {GURL("http://abcdef.com"), GURL("https://abcdef.com")},

      // The upgrade rule has a lower priority than the redirect rule matched so
      // the request should be redirected.
      {GURL("http://defghi.com"), GURL("http://example.com")},

      // The request should be redirected as it does not match the upgrade rule
      // because of its scheme.
      {GURL("https://abcdef.com"), GURL("http://google.com")},
      {GURL("http://xyz.com"), base::nullopt},
  };

  for (const auto& test_case : test_cases) {
    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    RedirectAction redirect_action =
        composite_matcher->ShouldRedirectRequest(params, PageAccess::kAllowed);

    if (test_case.expected_final_url) {
      EXPECT_EQ(test_case.expected_final_url->spec(),
                redirect_action.redirect_url->spec());
    } else
      EXPECT_FALSE(redirect_action.redirect_url);

    EXPECT_FALSE(redirect_action.notify_request_withheld);
  }
}

}  // namespace declarative_net_request
}  // namespace extensions
