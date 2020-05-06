// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

using PageAccess = PermissionsData::PageAccess;
using ActionInfo = CompositeMatcher::ActionInfo;

namespace dnr_api = api::declarative_net_request;

class CompositeMatcherTest : public ::testing::Test {
 public:
  CompositeMatcherTest() : channel_(::version_info::Channel::UNKNOWN) {}

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcherTest);
};

// Ensure that the rules in a CompositeMatcher are in the same priority space.
TEST_F(CompositeMatcherTest, SamePrioritySpace) {
  // Create the first ruleset matcher. It allows requests to google.com.
  TestRule allow_rule = CreateGenericRule();
  allow_rule.id = kMinValidID;
  allow_rule.condition->url_filter = std::string("google.com");
  allow_rule.action->type = std::string("allow");
  allow_rule.priority = 1;
  std::unique_ptr<RulesetMatcher> allow_matcher;
  RulesetID ruleset_id_one(1);
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule}, CreateTemporarySource(ruleset_id_one), &allow_matcher));

  // Now create the second matcher. It blocks requests to google.com, with
  // higher priority than the allow rule.
  TestRule block_rule = allow_rule;
  block_rule.action->type = std::string("block");
  block_rule.priority = 2;
  std::unique_ptr<RulesetMatcher> block_matcher;
  RulesetID ruleset_id_two(2);
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule}, CreateTemporarySource(ruleset_id_two), &block_matcher));

  // Create a composite matcher with both rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(allow_matcher));
  matchers.push_back(std::move(block_matcher));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  GURL google_url("http://google.com");
  RequestParams params;
  params.url = &google_url;

  // The block rule should be higher priority.
  ActionInfo action_info =
      composite_matcher->GetBeforeRequestAction(params, PageAccess::kAllowed);
  ASSERT_TRUE(action_info.action);
  EXPECT_EQ(action_info.action->type, RequestAction::Type::BLOCK);

  // Now swap the priority of the rules, which requires re-creating the ruleset
  // matchers and composite matcher.
  allow_rule.priority = 2;
  block_rule.priority = 1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule}, CreateTemporarySource(ruleset_id_one), &allow_matcher));
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule}, CreateTemporarySource(ruleset_id_two), &block_matcher));
  matchers.clear();
  matchers.push_back(std::move(allow_matcher));
  matchers.push_back(std::move(block_matcher));
  composite_matcher = std::make_unique<CompositeMatcher>(std::move(matchers));

  // The allow rule should now have higher priority.
  action_info =
      composite_matcher->GetBeforeRequestAction(params, PageAccess::kAllowed);
  ASSERT_TRUE(action_info.action);
  EXPECT_EQ(action_info.action->type, RequestAction::Type::ALLOW);
}

// Tests that header masks are correctly attributed to rules for multiple
// matchers in a CompositeMatcher.
TEST_F(CompositeMatcherTest, HeadersMaskForRules) {
  auto create_remove_headers_rule =
      [](int id, const std::string& url_filter,
         const std::vector<std::string>& remove_headers_list) {
        TestRule rule = CreateGenericRule();
        rule.id = id;
        rule.condition->url_filter = url_filter;
        rule.action->type = std::string("removeHeaders");
        rule.action->remove_headers_list = remove_headers_list;

        return rule;
      };

  TestRule static_rule_1 = create_remove_headers_rule(
      kMinValidID, "google.com", std::vector<std::string>({"cookie"}));

  TestRule dynamic_rule_1 = create_remove_headers_rule(
      kMinValidID, "/path", std::vector<std::string>({"referer"}));

  TestRule dynamic_rule_2 = create_remove_headers_rule(
      kMinValidID + 1, "/path", std::vector<std::string>({"setCookie"}));

  // Create the first ruleset matcher, which matches all requests from
  // |google.com|.
  const RulesetID kSource1ID(5);
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {static_rule_1}, CreateTemporarySource(kSource1ID), &matcher_1));

  // Create a second ruleset matcher, which matches all requests with |/path| in
  // their URL.
  const RulesetID kSource2ID = kDynamicRulesetID;
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher({dynamic_rule_1, dynamic_rule_2},
                                    CreateTemporarySource(kSource2ID),
                                    &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  GURL google_url = GURL("http://google.com/path");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  const uint8_t expected_mask = flat::RemoveHeaderType_referer |
                                flat::RemoveHeaderType_cookie |
                                flat::RemoveHeaderType_set_cookie;

  std::vector<RequestAction> actions;
  EXPECT_EQ(expected_mask, composite_matcher->GetRemoveHeadersMask(
                               google_params, 0u, &actions));

  // Construct expected request actions to be taken for a request to google.com.
  // Static actions are attributed to |matcher_1| and dynamic actions are
  // attributed to |matcher_2|.
  RequestAction static_action_1 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *static_rule_1.id, kDefaultPriority,
      kSource1ID);
  static_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kCookie);

  RequestAction dynamic_action_1 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *dynamic_rule_1.id, kDefaultPriority,
      kDynamicRulesetID);
  dynamic_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kReferer);

  RequestAction dynamic_action_2 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *dynamic_rule_2.id, kDefaultPriority,
      kDynamicRulesetID);
  dynamic_action_2.response_headers_to_remove.push_back("set-cookie");

  EXPECT_THAT(actions, ::testing::UnorderedElementsAre(
                           ::testing::Eq(::testing::ByRef(static_action_1)),
                           ::testing::Eq(::testing::ByRef(dynamic_action_1)),
                           ::testing::Eq(::testing::ByRef(dynamic_action_2))));
}

// Tests the GetModifyHeadersActions method.
TEST_F(CompositeMatcherTest, GetModifyHeadersActions) {
  auto create_modify_headers_rule =
      [](int id, int priority, const std::string& url_filter,
         std::vector<TestHeaderInfo> request_headers_list) {
        TestRule rule = CreateGenericRule();
        rule.id = id;
        rule.priority = priority;
        rule.condition->url_filter = url_filter;
        rule.action->type = std::string("modifyHeaders");
        rule.action->request_headers = std::move(request_headers_list);
        return rule;
      };

  TestRule rule_1 = create_modify_headers_rule(
      kMinValidID, kMinValidPriority, "google.com",
      std::vector<TestHeaderInfo>({TestHeaderInfo("header1", "remove"),
                                   TestHeaderInfo("header2", "remove")}));

  TestRule rule_2 = create_modify_headers_rule(
      kMinValidID, kMinValidPriority + 1, "/path",
      std::vector<TestHeaderInfo>({TestHeaderInfo("header1", "remove"),
                                   TestHeaderInfo("header3", "remove")}));

  // Create the first ruleset matcher, which matches all requests from
  // |google.com|.
  const RulesetID kSource1ID(1);
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1}, CreateTemporarySource(kSource1ID),
                                    &matcher_1));

  // Create a second ruleset matcher, which matches all requests with |/path| in
  // their URL.
  const RulesetID kSource2ID(2);
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_2}, CreateTemporarySource(kSource2ID),
                                    &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  GURL google_url = GURL("http://google.com/path");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // Call GetBeforeRequestAction first to ensure that test and production code
  // paths are consistent.
  composite_matcher->GetBeforeRequestAction(google_params,
                                            PageAccess::kAllowed);

  std::vector<RequestAction> actions =
      composite_matcher->GetModifyHeadersActions(google_params);

  // Construct expected request actions to be taken for a request to google.com.
  RequestAction action_1 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_1.id, *rule_1.priority, kSource1ID);
  action_1.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE),
      RequestAction::HeaderInfo("header2", dnr_api::HEADER_OPERATION_REMOVE)};

  RequestAction action_2 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_2.id, *rule_2.priority, kSource2ID);
  action_2.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE),
      RequestAction::HeaderInfo("header3", dnr_api::HEADER_OPERATION_REMOVE)};

  // |action_2| should be before |action_1| because |rule_2|
  // has a higher priority.
  EXPECT_THAT(actions, ::testing::ElementsAre(
                           ::testing::Eq(::testing::ByRef(action_2)),
                           ::testing::Eq(::testing::ByRef(action_1))));

  // Now swap the priority of the rules, which requires re-creating the ruleset
  // matchers and composite matcher.
  rule_1.priority = kMinValidPriority + 1;
  rule_2.priority = kMinValidPriority;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1}, CreateTemporarySource(kSource1ID),
                                    &matcher_1));
  ASSERT_TRUE(CreateVerifiedMatcher({rule_2}, CreateTemporarySource(kSource2ID),
                                    &matcher_2));

  matchers.clear();
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  composite_matcher = std::make_unique<CompositeMatcher>(std::move(matchers));

  // Call GetBeforeRequestAction first to ensure that test and production code
  // paths are consistent.
  composite_matcher->GetBeforeRequestAction(google_params,
                                            PageAccess::kAllowed);

  // Re-create |action_1| and |action_2| with the updated rule
  // priorities. The headers removed by each action should not change.
  actions = composite_matcher->GetModifyHeadersActions(google_params);
  action_1 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_1.id, *rule_1.priority, kSource1ID);
  action_1.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE),
      RequestAction::HeaderInfo("header2", dnr_api::HEADER_OPERATION_REMOVE)};

  action_2 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_2.id, *rule_2.priority, kSource2ID);
  action_2.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE),
      RequestAction::HeaderInfo("header3", dnr_api::HEADER_OPERATION_REMOVE)};

  // |action_1| should now be before |action_2| after their
  // priorities have been reversed.
  EXPECT_THAT(actions, ::testing::ElementsAre(
                           ::testing::Eq(::testing::ByRef(action_1)),
                           ::testing::Eq(::testing::ByRef(action_2))));
}

// Ensure CompositeMatcher detects requests to be notified based on the rule
// matched and whether the extenion has access to the request.
TEST_F(CompositeMatcherTest, NotifyWithholdFromPageAccess) {
  TestRule redirect_rule = CreateGenericRule();
  redirect_rule.condition->url_filter = std::string("google.com");
  redirect_rule.priority = kMinValidPriority;
  redirect_rule.action->type = std::string("redirect");
  redirect_rule.action->redirect.emplace();
  redirect_rule.action->redirect->url = std::string("http://ruleset1.com");
  redirect_rule.id = kMinValidID;

  TestRule upgrade_rule = CreateGenericRule();
  upgrade_rule.condition->url_filter = std::string("example.com");
  upgrade_rule.priority = kMinValidPriority + 1;
  upgrade_rule.action->type = std::string("upgradeScheme");
  upgrade_rule.id = kMinValidID + 1;

  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher({redirect_rule, upgrade_rule},
                                    CreateTemporarySource(), &matcher_1));

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
    SCOPED_TRACE(base::StringPrintf(
        "request_url=%s, access=%d, expected_final_url=%s, "
        "should_notify_withheld=%d",
        test_case.request_url.spec().c_str(), test_case.access,
        test_case.expected_final_url.value_or(GURL()).spec().c_str(),
        test_case.should_notify_withheld));

    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    ActionInfo redirect_action_info =
        composite_matcher->GetBeforeRequestAction(params, test_case.access);

    EXPECT_EQ(test_case.should_notify_withheld,
              redirect_action_info.notify_request_withheld);
  }
}

// Tests that the redirect url within an extension's ruleset is chosen based on
// the highest priority matching rule.
TEST_F(CompositeMatcherTest, GetRedirectUrlFromPriority) {
  TestRule abc_redirect = CreateGenericRule();
  abc_redirect.condition->url_filter = std::string("*abc*");
  abc_redirect.priority = kMinValidPriority;
  abc_redirect.action->type = std::string("redirect");
  abc_redirect.action->redirect.emplace();
  abc_redirect.action->redirect->url = std::string("http://google.com");
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
  ghi_redirect.action->redirect.emplace();
  ghi_redirect.action->redirect->url = std::string("http://example.com");
  ghi_redirect.id = kMinValidID + 2;

  // In terms of priority: ghi > def > abc.

  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher({abc_redirect, def_upgrade, ghi_redirect},
                                    CreateTemporarySource(), &matcher_1));

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

      // The request will not be redirected as it matches the upgrade rule but
      // is already https.
      {GURL("https://abcdef.com"), base::nullopt},

      {GURL("http://xyz.com"), base::nullopt},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "Test redirect from %s to %s", test_case.request_url.spec().c_str(),
        test_case.expected_final_url.value_or(GURL()).spec().c_str()));

    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    ActionInfo redirect_action_info =
        composite_matcher->GetBeforeRequestAction(params, PageAccess::kAllowed);

    if (test_case.expected_final_url) {
      ASSERT_TRUE(redirect_action_info.action);
      EXPECT_TRUE(redirect_action_info.action->IsRedirectOrUpgrade());
      EXPECT_EQ(test_case.expected_final_url,
                redirect_action_info.action->redirect_url);
    } else {
      EXPECT_FALSE(redirect_action_info.action.has_value());
    }

    EXPECT_FALSE(redirect_action_info.notify_request_withheld);
  }
}

}  // namespace declarative_net_request
}  // namespace extensions
