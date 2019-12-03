// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
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
    return !matcher->GetAllowAction(params) &&
           matcher->GetBlockOrCollapseAction(params).has_value();
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
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("http://yahoo.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  base::Optional<RequestAction> redirect_action =
      matcher->GetRedirectAction(params);
  ASSERT_TRUE(redirect_action);
  EXPECT_EQ(yahoo_url, redirect_action->redirect_url);

  params.url = &yahoo_url;
  EXPECT_FALSE(matcher->GetRedirectAction(params));
}

// Test that a URL cannot redirect to itself, as filed in crbug.com/954646.
TEST_F(RulesetMatcherTest, PreventSelfRedirect) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("go*");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("http://google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL url("http://google.com");
  RequestParams params;
  params.url = &url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  EXPECT_FALSE(matcher->GetRedirectAction(params));
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
    return matcher->GetUpgradeAction(params).has_value();
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
  std::vector<RequestAction> remove_header_actions;

  RequestParams params;
  params.url = &example_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;
  EXPECT_EQ(0u, matcher->GetRemoveHeadersMask(
                    params, 0u /* excluded_remove_headers_mask */,
                    &remove_header_actions));
  EXPECT_TRUE(remove_header_actions.empty());

  rule.action->type = std::string("removeHeaders");
  rule.action->remove_headers_list =
      std::vector<std::string>({"referer", "cookie", "setCookie"});
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());
  const uint8_t expected_mask = flat::RemoveHeaderType_referer |
                                flat::RemoveHeaderType_cookie |
                                flat::RemoveHeaderType_set_cookie;

  EXPECT_EQ(expected_mask, matcher->GetRemoveHeadersMask(
                               params, 0u /* excluded_remove_headers_mask */,
                               &remove_header_actions));

  RequestAction expected_action =
      CreateRequestActionForTesting(RequestAction::Type::REMOVE_HEADERS);
  expected_action.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kCookie);
  expected_action.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kReferer);
  expected_action.response_headers_to_remove.push_back("set-cookie");

  ASSERT_EQ(1u, remove_header_actions.size());
  EXPECT_EQ(expected_action, remove_header_actions[0]);

  remove_header_actions.clear();

  GURL google_url("http://google.com");
  params.url = &google_url;
  EXPECT_EQ(0u, matcher->GetRemoveHeadersMask(
                    params, 0u /* excluded_remove_headers_mask */,
                    &remove_header_actions));
  EXPECT_TRUE(remove_header_actions.empty());

  uint8_t excluded_remove_headers_mask =
      flat::RemoveHeaderType_referer | flat::RemoveHeaderType_set_cookie;
  EXPECT_EQ(0u,
            matcher->GetRemoveHeadersMask(params, excluded_remove_headers_mask,
                                          &remove_header_actions));
  EXPECT_TRUE(remove_header_actions.empty());

  // The current mask is ignored while matching and is not returned as part of
  // the result.
  params.url = &example_url;
  EXPECT_EQ(flat::RemoveHeaderType_cookie,
            matcher->GetRemoveHeadersMask(params, excluded_remove_headers_mask,
                                          &remove_header_actions));

  expected_action.request_headers_to_remove.clear();
  expected_action.response_headers_to_remove.clear();
  expected_action.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kCookie);

  ASSERT_EQ(1u, remove_header_actions.size());
  EXPECT_EQ(expected_action, remove_header_actions[0]);
}

// Tests that GetRemoveHeadersMask for multiple rules will return one
// RequestAction per matching rule.
TEST_F(RulesetMatcherTest, RemoveHeadersMultipleRules) {
  TestRule rule_1 = CreateGenericRule();
  rule_1.condition->url_filter = std::string("example.com");
  rule_1.action->type = std::string("removeHeaders");
  rule_1.action->remove_headers_list = std::vector<std::string>({"referer"});

  TestRule rule_2 = CreateGenericRule();
  rule_2.id = kMinValidID + 1;
  rule_2.condition->url_filter = std::string("example.com");
  rule_2.action->type = std::string("removeHeaders");
  rule_2.action->remove_headers_list = std::vector<std::string>({"cookie"});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1, rule_2}, CreateTemporarySource(),
                                    &matcher));
  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  RequestAction rule_1_action = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *rule_1.id);
  rule_1_action.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kReferer);

  RequestAction rule_2_action = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *rule_2.id);
  rule_2_action.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kCookie);

  std::vector<RequestAction> remove_header_actions;
  EXPECT_EQ(flat::RemoveHeaderType_cookie | flat::RemoveHeaderType_referer,
            matcher->GetRemoveHeadersMask(params,
                                          0u /* excluded_remove_headers_mask */,
                                          &remove_header_actions));

  EXPECT_THAT(remove_header_actions,
              ::testing::UnorderedElementsAre(
                  ::testing::Eq(::testing::ByRef(rule_1_action)),
                  ::testing::Eq(::testing::ByRef(rule_2_action))));
}

// Tests a rule to redirect to an extension path.
TEST_F(RulesetMatcherTest, RedirectToExtensionPath) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  rule.action->redirect.emplace();
  rule.action->redirect->extension_path = "/path/newfile.js?query#fragment";

  std::unique_ptr<RulesetMatcher> matcher;
  const size_t kId = 1;
  const size_t kPriority = 1;
  const size_t kRuleCountLimit = 10;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {rule},
      CreateTemporarySource(kId, kPriority,
                            api::declarative_net_request::SOURCE_TYPE_MANIFEST,
                            kRuleCountLimit),
      &matcher));

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;

  base::Optional<RequestAction> redirect_action =
      matcher->GetRedirectAction(params);

  ASSERT_TRUE(redirect_action.has_value());
  GURL expected_redirect_url(
      "chrome-extension://extensionid/path/newfile.js?query#fragment");
  EXPECT_EQ(expected_redirect_url, redirect_action->redirect_url);
}

// Tests a rule to redirect to a static url.
TEST_F(RulesetMatcherTest, RedirectToStaticUrl) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  rule.action->redirect.emplace();
  rule.action->redirect->url = "https://google.com";

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;

  base::Optional<RequestAction> redirect_action =
      matcher->GetRedirectAction(params);

  ASSERT_TRUE(redirect_action.has_value());
  GURL expected_redirect_url("https://google.com");
  EXPECT_EQ(expected_redirect_url, redirect_action->redirect_url);
}

// Tests url transformation rules.
TEST_F(RulesetMatcherTest, UrlTransform) {
  struct TestCase {
    std::string url;
    // Valid if a redirect is expected.
    base::Optional<std::string> expected_redirect_url;
  };

  std::vector<TestCase> cases;
  std::vector<TestRule> rules;

  auto create_transform_rule = [](size_t id, const std::string& filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter = filter;
    rule.priority = kMinValidPriority;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->transform.emplace();
    return rule;
  };

  TestRule rule = create_transform_rule(1, "||1.com");
  rule.action->redirect->transform->scheme = "https";
  rules.push_back(rule);
  cases.push_back({"http://1.com/path?query", "https://1.com/path?query"});

  rule = create_transform_rule(2, "||2.com");
  rule.action->redirect->transform->scheme = "ftp";
  rule.action->redirect->transform->host = "ftp.host.com";
  rule.action->redirect->transform->port = "70";
  rules.push_back(rule);
  cases.push_back(
      {"http://2.com:100/path?query", "ftp://ftp.host.com:70/path?query"});

  rule = create_transform_rule(3, "||3.com");
  rule.action->redirect->transform->port = "";
  rule.action->redirect->transform->path = "";
  rule.action->redirect->transform->query = "?new_query";
  rule.action->redirect->transform->fragment = "#fragment";
  rules.push_back(rule);
  // The path separator '/' is output even when cleared, except when there is no
  // authority, query and fragment.
  cases.push_back(
      {"http://3.com:100/path?query", "http://3.com/?new_query#fragment"});

  rule = create_transform_rule(4, "||4.com");
  rule.action->redirect->transform->scheme = "http";
  rule.action->redirect->transform->path = " ";
  rules.push_back(rule);
  cases.push_back({"http://4.com/xyz", "http://4.com/%20"});

  rule = create_transform_rule(5, "||5.com");
  rule.action->redirect->transform->path = "/";
  rule.action->redirect->transform->query = "";
  rule.action->redirect->transform->fragment = "#";
  rules.push_back(rule);
  cases.push_back(
      {"http://5.com:100/path?query#fragment", "http://5.com:100/#"});

  rule = create_transform_rule(6, "||6.com");
  rule.action->redirect->transform->path = "/path?query";
  rules.push_back(rule);
  // The "?" in path is url encoded since it's not part of the query.
  cases.push_back({"http://6.com/xyz?q1", "http://6.com/path%3Fquery?q1"});

  rule = create_transform_rule(7, "||7.com");
  rule.action->redirect->transform->username = "new_user";
  rule.action->redirect->transform->password = "new_pass";
  rules.push_back(rule);
  cases.push_back(
      {"http://user@7.com/xyz", "http://new_user:new_pass@7.com/xyz"});

  auto make_query = [](const std::string& key, const std::string& value) {
    TestRuleQueryKeyValue query;
    query.key = key;
    query.value = value;
    return query;
  };

  rule = create_transform_rule(8, "||8.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"r1", "r2"});
  rule.action->redirect->transform->query_transform->add_or_replace_params =
      std::vector<TestRuleQueryKeyValue>(
          {make_query("a1", "#"), make_query("a2", ""),
           make_query("a1", "new2"), make_query("a1", "new3")});
  rules.push_back(rule);
  cases.push_back(
      {"http://8.com/"
       "path?r1&r1=val1&a1=val1&r2=val&x3=val&a1=val2&a2=val&r1=val2",
       "http://8.com/path?a1=%23&x3=val&a1=new2&a2=&a1=new3"});
  cases.push_back({"http://8.com/path?query",
                   "http://8.com/path?query=&a1=%23&a2=&a1=new2&a1=new3"});

  rule = create_transform_rule(9, "||9.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"r1", "r2"});
  rules.push_back(rule);
  // No redirect is performed since the url won't change.
  cases.push_back({"http://9.com/path?query#fragment", base::nullopt});

  rule = create_transform_rule(10, "||10.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"q1"});
  rule.action->redirect->transform->query_transform->add_or_replace_params =
      std::vector<TestRuleQueryKeyValue>({make_query("q1", "new")});
  rules.push_back(rule);
  cases.push_back(
      {"https://10.com/path?q1=1&q1=2&q1=3", "https://10.com/path?q1=new"});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing url %s", test_case.url.c_str()));

    GURL url(test_case.url);
    ASSERT_TRUE(url.is_valid()) << test_case.url;
    RequestParams params;
    params.url = &url;

    base::Optional<RequestAction> redirect_action =
        matcher->GetRedirectAction(params);

    if (!test_case.expected_redirect_url) {
      EXPECT_FALSE(redirect_action) << redirect_action->redirect_url->spec();
      continue;
    }

    ASSERT_TRUE(GURL(*test_case.expected_redirect_url).is_valid())
        << *test_case.expected_redirect_url;

    ASSERT_TRUE(redirect_action.has_value());
    EXPECT_EQ(GURL(*test_case.expected_redirect_url),
              redirect_action->redirect_url);
  }
}

// Tests regex rules are evaluated correctly for different action types.
TEST_F(RulesetMatcherTest, RegexRules) {
  auto create_regex_rule = [](size_t id, const std::string& regex_filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = regex_filter;
    return rule;
  };

  std::vector<TestRule> rules;

  // Add a blocking rule.
  TestRule block_rule = create_regex_rule(1, R"((?:block|collapse)\.com/path)");
  rules.push_back(block_rule);

  // Add an allowlist rule.
  TestRule allow_rule = create_regex_rule(2, R"(http://(\w+\.)+allow\.com)");
  allow_rule.action->type = "allow";
  rules.push_back(allow_rule);

  // Add a redirect rule.
  TestRule redirect_rule = create_regex_rule(3, R"(redirect\.com)");
  redirect_rule.action->type = "redirect";
  redirect_rule.action->redirect.emplace();
  redirect_rule.priority = kMinValidPriority;
  redirect_rule.action->redirect->url = "https://google.com";
  rules.push_back(redirect_rule);

  // Add a upgrade rule.
  TestRule upgrade_rule = create_regex_rule(4, "upgrade");
  upgrade_rule.action->type = "upgradeScheme";
  upgrade_rule.priority = kMinValidPriority;
  rules.push_back(upgrade_rule);

  // Add a remove headers rule.
  TestRule remove_headers_rule =
      create_regex_rule(5, R"(^(?:http|https)://[a-z\.]+\.in)");
  remove_headers_rule.action->type = "removeHeaders";
  remove_headers_rule.action->remove_headers_list =
      std::vector<std::string>({"referer", "cookie"});
  rules.push_back(remove_headers_rule);

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  struct TestCase {
    const char* url = nullptr;
    base::Optional<RequestAction> expected_block_or_collapse_action;
    base::Optional<RequestAction> expected_allow_action;
    base::Optional<RequestAction> expected_redirect_action;
    base::Optional<RequestAction> expected_upgrade_action;
    uint8_t expected_remove_headers_mask = 0u;
    base::Optional<RequestAction> expected_remove_header_action;
  };

  std::vector<TestCase> test_cases;

  {
    TestCase test_case = {"http://www.block.com/path"};
    test_case.expected_block_or_collapse_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *block_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://www.collapse.com/PATH"};
    test_cases.push_back(std::move(test_case));
    // Filters are case sensitive by default, hence the request doesn't match.
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.xyz.allow.com/path"};
    test_case.expected_allow_action = CreateRequestActionForTesting(
        RequestAction::Type::ALLOW, *allow_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://allow.com/path"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://redirect.com?path=abc"};
    test_case.expected_redirect_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, *redirect_rule.id);
    test_case.expected_redirect_action->redirect_url =
        GURL(*redirect_rule.action->redirect->url);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://redirect.eu#query"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/upgrade"};
    test_case.expected_upgrade_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, *upgrade_rule.id);
    test_case.expected_upgrade_action->redirect_url.emplace(
        "https://example.com/upgrade");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.in/path"};
    test_case.expected_remove_headers_mask =
        flat::RemoveHeaderType_cookie | flat::RemoveHeaderType_referer;
    test_case.expected_remove_header_action = CreateRequestActionForTesting(
        RequestAction::Type::REMOVE_HEADERS, *remove_headers_rule.id);
    test_case.expected_remove_header_action->request_headers_to_remove = {
        net::HttpRequestHeaders::kCookie, net::HttpRequestHeaders::kReferer};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc123.in/path"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com"};
    test_cases.push_back(std::move(test_case));
  }

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);

    GURL url(test_case.url);
    RequestParams params;
    params.url = &url;

    EXPECT_EQ(test_case.expected_block_or_collapse_action,
              matcher->GetBlockOrCollapseAction(params));
    EXPECT_EQ(test_case.expected_allow_action, matcher->GetAllowAction(params));
    EXPECT_EQ(test_case.expected_redirect_action,
              matcher->GetRedirectAction(params));
    EXPECT_EQ(test_case.expected_upgrade_action,
              matcher->GetUpgradeAction(params));

    std::vector<RequestAction> remove_header_actions;
    EXPECT_EQ(test_case.expected_remove_headers_mask,
              matcher->GetRemoveHeadersMask(
                  params, 0u /* excluded_remove_headers_mask */,
                  &remove_header_actions));
    if (test_case.expected_remove_header_action) {
      EXPECT_THAT(remove_header_actions,
                  testing::ElementsAre(testing::Eq(testing::ByRef(
                      test_case.expected_remove_header_action))));
    } else {
      EXPECT_TRUE(remove_header_actions.empty());
    }
  }

  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());
}

// Ensure that the rule metadata is checked correctly for regex rules.
TEST_F(RulesetMatcherTest, RegexRules_Metadata) {
  auto create_regex_rule = [](size_t id, const std::string& regex_filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = regex_filter;
    return rule;
  };

  std::vector<TestRule> rules;

  // Add a case sensitive rule.
  TestRule path_rule = create_regex_rule(1, "/PATH");
  rules.push_back(path_rule);

  // Add a case insensitive rule.
  TestRule xyz_rule = create_regex_rule(2, "/XYZ");
  xyz_rule.condition->is_url_filter_case_sensitive = false;
  rules.push_back(xyz_rule);

  // Test |domains|, |excludedDomains|.
  TestRule google_rule = create_regex_rule(3, "google");
  google_rule.condition->domains = std::vector<std::string>({"example.com"});
  google_rule.condition->excluded_domains =
      std::vector<std::string>({"b.example.com"});
  rules.push_back(google_rule);

  // Test |resourceTypes|.
  TestRule sub_frame_rule = create_regex_rule(4, R"((abc|def)\.com)");
  sub_frame_rule.condition->resource_types =
      std::vector<std::string>({"sub_frame"});
  rules.push_back(sub_frame_rule);

  // Test |domainType|.
  TestRule third_party_rule = create_regex_rule(5, R"(http://(\d+)\.com)");
  third_party_rule.condition->domain_type = "thirdParty";
  rules.push_back(third_party_rule);

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  struct TestCase {
    const char* url = nullptr;
    url::Origin first_party_origin;
    url_pattern_index::flat::ElementType element_type =
        url_pattern_index::flat::ElementType_OTHER;
    bool is_third_party = false;
    base::Optional<RequestAction> expected_block_or_collapse_action;
  };
  std::vector<TestCase> test_cases;

  {
    TestCase test_case = {"http://example.com/path/abc"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/PATH/abc"};
    test_case.expected_block_or_collapse_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *path_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/xyz/abc"};
    test_case.expected_block_or_collapse_action =
        CreateRequestActionForTesting(RequestAction::Type::BLOCK, *xyz_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/XYZ/abc"};
    test_case.expected_block_or_collapse_action =
        CreateRequestActionForTesting(RequestAction::Type::BLOCK, *xyz_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/google"};
    test_case.first_party_origin =
        url::Origin::Create(GURL("http://a.example.com"));
    test_case.is_third_party = true;
    test_case.expected_block_or_collapse_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *google_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/google"};
    test_case.first_party_origin =
        url::Origin::Create(GURL("http://b.example.com"));
    test_case.is_third_party = true;
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.com"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.com"};
    test_case.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    test_case.expected_block_or_collapse_action = CreateRequestActionForTesting(
        RequestAction::Type::COLLAPSE, *sub_frame_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://243.com"};
    test_case.is_third_party = true;
    test_case.expected_block_or_collapse_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *third_party_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://243.com"};
    test_case.first_party_origin = url::Origin::Create(GURL(test_case.url));
    test_case.is_third_party = false;
    test_cases.push_back(std::move(test_case));
  }

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Case number-%" PRIuS " url-%s", i,
                                    test_cases[i].url));

    GURL url(test_cases[i].url);
    RequestParams params;
    params.url = &url;
    params.first_party_origin = test_cases[i].first_party_origin;
    params.element_type = test_cases[i].element_type;
    params.is_third_party = test_cases[i].is_third_party;

    EXPECT_EQ(test_cases[i].expected_block_or_collapse_action,
              matcher->GetBlockOrCollapseAction(params));
  }
}

// Ensures that RulesetMatcher combines the results of regex and filter-list
// style redirect rules correctly.
TEST_F(RulesetMatcherTest, RegexAndFilterListRules_RedirectPriority) {
  struct {
    size_t id;
    size_t priority;
    const char* action_type;
    const char* filter;
    bool is_regex_rule;
    base::Optional<std::string> redirect_url;
  } rule_info[] = {
      {1, 1, "redirect", "filter.com", false, "http://redirect_filter.com"},
      {2, 1, "upgradeScheme", "regex\\.com", true, base::nullopt},
      {3, 9, "redirect", "common1.com", false, "http://common1_filter.com"},
      {4, 10, "redirect", "common1\\.com", true, "http://common1_regex.com"},
      {5, 10, "upgradeScheme", "common2.com", false, base::nullopt},
      {6, 9, "upgradeScheme", "common2\\.com", true, base::nullopt},
      {7, 10, "redirect", "abc\\.com", true, "http://example1.com"},
      {8, 9, "redirect", "abc", true, "http://example2.com"},
  };

  std::vector<TestRule> rules;
  for (const auto& info : rule_info) {
    TestRule rule = CreateGenericRule();
    rule.id = info.id;
    rule.priority = info.priority;
    rule.action->type = info.action_type;

    rule.condition->url_filter.reset();
    if (info.is_regex_rule)
      rule.condition->regex_filter = info.filter;
    else
      rule.condition->url_filter = info.filter;

    if (info.redirect_url) {
      rule.action->redirect.emplace();
      rule.action->redirect->url = info.redirect_url;
    }

    rules.push_back(rule);
  }

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  struct TestCase {
    const char* url = nullptr;
    base::Optional<RequestAction> expected_redirect_action;
    base::Optional<RequestAction> expected_upgrade_action;
  };
  std::vector<TestCase> test_cases;

  {
    TestCase test_case = {"http://filter.com"};
    test_case.expected_redirect_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[0].id, rule_info[0].priority);
    test_case.expected_redirect_action->redirect_url.emplace(
        "http://redirect_filter.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://regex.com"};
    test_case.expected_upgrade_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[1].id, rule_info[1].priority);
    test_case.expected_upgrade_action->redirect_url.emplace(
        "https://regex.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://common1.com"};
    test_case.expected_redirect_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[3].id, rule_info[3].priority);
    test_case.expected_redirect_action->redirect_url.emplace(
        "http://common1_regex.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://common2.com"};
    test_case.expected_upgrade_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[4].id, rule_info[4].priority);
    test_case.expected_upgrade_action->redirect_url.emplace(
        "https://common2.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"https://common2.com"};
    // No action since request is not upgradeable.
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.com"};
    test_case.expected_redirect_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[6].id, rule_info[6].priority);
    test_case.expected_redirect_action->redirect_url.emplace(
        "http://example1.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://xyz.com/abc"};
    test_case.expected_redirect_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[7].id, rule_info[7].priority);
    test_case.expected_redirect_action->redirect_url.emplace(
        "http://example2.com");
    test_cases.push_back(std::move(test_case));
  }

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);

    GURL url(test_case.url);
    RequestParams params;
    params.url = &url;

    EXPECT_EQ(test_case.expected_redirect_action,
              matcher->GetRedirectAction(params));
    EXPECT_EQ(test_case.expected_upgrade_action,
              matcher->GetUpgradeAction(params));
  }
}

// Ensures that RulesetMatcher combines the results of regex and filter-list
// style remove headers rules correctly.
TEST_F(RulesetMatcherTest, RegexAndFilterListRules_RemoveHeaders) {
  std::vector<TestRule> rules;

  TestRule rule = CreateGenericRule();
  rule.id = 1;
  rule.action->type = "removeHeaders";
  rule.condition->url_filter = "abc";
  rule.action->remove_headers_list = std::vector<std::string>({"cookie"});
  rules.push_back(rule);
  RequestAction action_1 =
      CreateRequestActionForTesting(RequestAction::Type::REMOVE_HEADERS, 1);
  action_1.request_headers_to_remove = {net::HttpRequestHeaders::kCookie};

  rule = CreateGenericRule();
  rule.id = 2;
  rule.condition->url_filter.reset();
  rule.condition->regex_filter = "example";
  rule.action->type = "removeHeaders";
  rule.action->remove_headers_list =
      std::vector<std::string>({"cookie", "setCookie"});
  rules.push_back(rule);
  RequestAction action_2 =
      CreateRequestActionForTesting(RequestAction::Type::REMOVE_HEADERS, 2);
  action_2.request_headers_to_remove = {net::HttpRequestHeaders::kCookie};
  action_2.response_headers_to_remove = {"set-cookie"};

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  {
    GURL url("http://abc.com");
    SCOPED_TRACE(url.spec());
    RequestParams params;
    params.url = &url;
    std::vector<RequestAction> actions;
    EXPECT_EQ(flat::RemoveHeaderType_cookie,
              matcher->GetRemoveHeadersMask(
                  params, 0 /* excluded_remove_headers_mask */, &actions));
    EXPECT_THAT(actions, testing::UnorderedElementsAre(
                             testing::Eq(testing::ByRef(action_1))));
  }

  {
    GURL url("http://example.com");
    SCOPED_TRACE(url.spec());
    RequestParams params;
    params.url = &url;
    std::vector<RequestAction> actions;
    EXPECT_EQ(flat::RemoveHeaderType_cookie | flat::RemoveHeaderType_set_cookie,
              matcher->GetRemoveHeadersMask(
                  params, 0 /* excluded_remove_headers_mask */, &actions));
    EXPECT_THAT(actions, testing::UnorderedElementsAre(
                             testing::Eq(testing::ByRef(action_2))));
  }

  {
    GURL url("http://abc.com/example");
    SCOPED_TRACE(url.spec());
    RequestParams params;
    params.url = &url;
    std::vector<RequestAction> actions;
    EXPECT_EQ(flat::RemoveHeaderType_cookie | flat::RemoveHeaderType_set_cookie,
              matcher->GetRemoveHeadersMask(
                  params, 0 /* excluded_remove_headers_mask */, &actions));

    // Removal of the cookie header will be attributed to rule 1 since filter
    // list style rules are evaluated first for efficiency reasons. (Note this
    // is an internal implementation detail). Hence only the set-cookie header
    // removal will be attributed to rule 2.
    action_2.request_headers_to_remove = {};
    EXPECT_THAT(actions, testing::UnorderedElementsAre(
                             testing::Eq(testing::ByRef(action_1)),
                             testing::Eq(testing::ByRef(action_2))));
  }
}

// Tests that regex substitution works correctly.
TEST_F(RulesetMatcherTest, RegexSubstitution) {
  struct {
    int id;
    std::string regex_filter;
    std::string regex_substitution;
  } rule_info[] = {
      // "\0" captures the complete matched string.
      {1, R"(^.*google\.com.*$)", R"(https://redirect.com?original=\0)"},
      {2, R"(http://((?:abc|def)\.xyz.com.*))", R"(https://www.\1)"},
      {3, R"((http|https)://example.com.*(\?|&)redirect=(.*?)(?:&|$))",
       R"(\1://\3)"},
      {4, R"(reddit\.com)", "https://abc.com"}};

  std::vector<TestRule> rules;
  for (const auto& info : rule_info) {
    TestRule rule = CreateGenericRule();
    rule.id = info.id;
    rule.priority = kMinValidPriority;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = info.regex_filter;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->regex_substitution = info.regex_substitution;
    rules.push_back(rule);
  }

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  auto create_redirect_action = [](int rule_id, std::string redirect_url) {
    RequestAction action =
        CreateRequestActionForTesting(RequestAction::Type::REDIRECT, rule_id);
    action.redirect_url.emplace(redirect_url);
    return action;
  };

  struct {
    std::string url;
    base::Optional<RequestAction> expected_redirect_action;
  } test_cases[] = {
      {"http://google.com/path?query",
       create_redirect_action(
           1, "https://redirect.com?original=http://google.com/path?query")},
      {"http://def.xyz.com/path?query",
       create_redirect_action(2, "https://www.def.xyz.com/path?query")},
      {"http://example.com/path?q1=1&redirect=facebook.com&q2=2",
       create_redirect_action(3, "http://facebook.com")},
      // The redirect url here would have been "http://" which is invalid.
      {"http://example.com/path?q1=1&redirect=&q2=2", base::nullopt},
      {"https://reddit.com", create_redirect_action(4, "https://abc.com")},
      // No match.
      {"http://example.com", base::nullopt}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);

    GURL url(test_case.url);
    CHECK(url.is_valid());
    RequestParams params;
    params.url = &url;

    ASSERT_EQ(test_case.expected_redirect_action,
              matcher->GetRedirectAction(params));
  }
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
