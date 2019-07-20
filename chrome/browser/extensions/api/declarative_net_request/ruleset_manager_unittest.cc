// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {

// Note: This is not declared in the anonymous namespace so that we can use it
// with gtest.
bool operator==(const RulesetManager::Action& lhs,
                const RulesetManager::Action& rhs) {
  static_assert(flat::ActionIndex_count == 7,
                "Modify this method to ensure it stays updated as new actions "
                "are added.");

  auto are_vectors_equal = [](std::vector<const char*> a,
                              std::vector<const char*> b) {
    return std::set<base::StringPiece>(a.begin(), a.end()) ==
           std::set<base::StringPiece>(b.begin(), b.end());
  };

  return lhs.type == rhs.type && lhs.redirect_url == rhs.redirect_url &&
         are_vectors_equal(lhs.request_headers_to_remove,
                           rhs.request_headers_to_remove) &&
         are_vectors_equal(lhs.response_headers_to_remove,
                           rhs.response_headers_to_remove);
}

namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

class RulesetManagerTest : public DNRTestBase {
 public:
  RulesetManagerTest() {}

  void SetUp() override {
    DNRTestBase::SetUp();
    manager_ = std::make_unique<RulesetManager>(browser_context());
  }

 protected:
  using Action = RulesetManager::Action;
  using ActionType = Action::Type;

  // Helper to create a composite matcher instance for the given |rules|.
  void CreateMatcherForRules(
      const std::vector<TestRule>& rules,
      const std::string& extension_dirname,
      std::unique_ptr<CompositeMatcher>* matcher,
      const std::vector<std::string>& host_permissions = {},
      bool has_background_script = false) {
    base::FilePath extension_dir =
        temp_dir().GetPath().AppendASCII(extension_dirname);

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir));
    WriteManifestAndRuleset(extension_dir, kJSONRulesetFilepath,
                            kJSONRulesFilename, rules, host_permissions,
                            has_background_script);

    last_loaded_extension_ =
        CreateExtensionLoader()->LoadExtension(extension_dir);
    ASSERT_TRUE(last_loaded_extension_);

    ExtensionRegistry::Get(browser_context())
        ->AddEnabled(last_loaded_extension_);

    int expected_checksum;
    EXPECT_TRUE(ExtensionPrefs::Get(browser_context())
                    ->GetDNRRulesetChecksum(last_loaded_extension_->id(),
                                            &expected_checksum));

    std::vector<std::unique_ptr<RulesetMatcher>> matchers(1);
    EXPECT_EQ(RulesetMatcher::kLoadSuccess,
              RulesetMatcher::CreateVerifiedMatcher(
                  RulesetSource::CreateStatic(*last_loaded_extension_),
                  expected_checksum, &matchers[0]));
    *matcher = std::make_unique<CompositeMatcher>(std::move(matchers));
  }

  void SetIncognitoEnabled(const Extension* extension, bool incognito_enabled) {
    util::SetIsIncognitoEnabled(extension->id(), browser_context(),
                                incognito_enabled);
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  }

  const Extension* last_loaded_extension() const {
    return last_loaded_extension_.get();
  }

  // Returns renderer-initiated request params for the given |url|.
  WebRequestInfoInitParams GetRequestParamsForURL(
      base::StringPiece url,
      base::Optional<url::Origin> initiator = base::nullopt) {
    const int kRendererId = 1;
    WebRequestInfoInitParams info;
    info.url = GURL(url);
    info.render_process_id = kRendererId;
    info.initiator = std::move(initiator);
    return info;
  }

  RulesetManager* manager() { return manager_.get(); }

 private:
  scoped_refptr<const Extension> last_loaded_extension_;
  std::unique_ptr<RulesetManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(RulesetManagerTest);
};

// Tests that the RulesetManager handles multiple rulesets correctly.
TEST_P(RulesetManagerTest, MultipleRulesets) {
  enum RulesetMask {
    kEnableRulesetOne = 1 << 0,
    kEnableRulesetTwo = 1 << 1,
  };

  TestRule rule_one = CreateGenericRule();
  rule_one.condition->url_filter = std::string("one.com");

  TestRule rule_two = CreateGenericRule();
  rule_two.condition->url_filter = std::string("two.com");

  auto should_block_request = [this](const WebRequestInfo& request) {
    return manager()->EvaluateRequest(request,
                                      false /*is_incognito_context*/) ==
           Action(ActionType::BLOCK);
  };

  for (int mask = 0; mask < 4; mask++) {
    SCOPED_TRACE(base::StringPrintf("Testing ruleset mask %d", mask));

    ASSERT_EQ(0u, manager()->GetMatcherCountForTest());

    std::string extension_id_one, extension_id_two;
    size_t expected_matcher_count = 0;

    // Add the required rulesets.
    if (mask & kEnableRulesetOne) {
      ++expected_matcher_count;
      std::unique_ptr<CompositeMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_one}, std::to_string(mask) + "_one", &matcher));
      extension_id_one = last_loaded_extension()->id();
      manager()->AddRuleset(extension_id_one, std::move(matcher),
                            URLPatternSet());
    }
    if (mask & kEnableRulesetTwo) {
      ++expected_matcher_count;
      std::unique_ptr<CompositeMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_two}, std::to_string(mask) + "_two", &matcher));
      extension_id_two = last_loaded_extension()->id();
      manager()->AddRuleset(extension_id_two, std::move(matcher),
                            URLPatternSet());
    }

    ASSERT_EQ(expected_matcher_count, manager()->GetMatcherCountForTest());

    WebRequestInfo request_one_info(GetRequestParamsForURL("http://one.com"));
    WebRequestInfo request_two_info(GetRequestParamsForURL("http://two.com"));
    WebRequestInfo request_three_info(
        GetRequestParamsForURL("http://three.com"));

    EXPECT_EQ((mask & kEnableRulesetOne) != 0,
              should_block_request(request_one_info));
    EXPECT_EQ((mask & kEnableRulesetTwo) != 0,
              should_block_request(request_two_info));
    EXPECT_FALSE(should_block_request(request_three_info));

    // Remove the rulesets.
    if (mask & kEnableRulesetOne)
      manager()->RemoveRuleset(extension_id_one);
    if (mask & kEnableRulesetTwo)
      manager()->RemoveRuleset(extension_id_two);
  }
}

// Tests that only extensions enabled in incognito mode can modify requests made
// from the incognito context.
TEST_P(RulesetManagerTest, IncognitoRequests) {
  // Add an extension ruleset blocking "example.com".
  TestRule rule_one = CreateGenericRule();
  rule_one.condition->url_filter = std::string("example.com");
  std::unique_ptr<CompositeMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule_one}, "test_extension", &matcher));
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                        URLPatternSet());

  WebRequestInfo request_info(GetRequestParamsForURL("http://example.com"));

  // By default, the extension is disabled in incognito mode. So requests from
  // incognito contexts should not be evaluated.
  EXPECT_FALSE(util::IsIncognitoEnabled(last_loaded_extension()->id(),
                                        browser_context()));
  EXPECT_EQ(
      Action(ActionType::NONE),
      manager()->EvaluateRequest(request_info, true /*is_incognito_context*/));
  request_info.dnr_action.reset();
  EXPECT_EQ(
      Action(ActionType::BLOCK),
      manager()->EvaluateRequest(request_info, false /*is_incognito_context*/));
  request_info.dnr_action.reset();

  // Enabling the extension in incognito mode, should cause requests from
  // incognito contexts to also be evaluated.
  SetIncognitoEnabled(last_loaded_extension(), true /*incognito_enabled*/);
  EXPECT_TRUE(util::IsIncognitoEnabled(last_loaded_extension()->id(),
                                       browser_context()));
  EXPECT_EQ(
      Action(ActionType::BLOCK),
      manager()->EvaluateRequest(request_info, true /*is_incognito_context*/));
  request_info.dnr_action.reset();
  EXPECT_EQ(
      Action(ActionType::BLOCK),
      manager()->EvaluateRequest(request_info, false /*is_incognito_context*/));
  request_info.dnr_action.reset();
}

// Tests that
// Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions2
// is only emitted when there are active rulesets.
TEST_P(RulesetManagerTest, TotalEvaluationTimeHistogram) {
  WebRequestInfo example_com_request(
      GetRequestParamsForURL("http://example.com"));
  WebRequestInfo google_com_request(
      GetRequestParamsForURL("http://google.com"));
  bool is_incognito_context = false;
  const char* kHistogramName =
      "Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions2";
  {
    base::HistogramTester tester;
    EXPECT_EQ(
        Action(ActionType::NONE),
        manager()->EvaluateRequest(example_com_request, is_incognito_context));
    EXPECT_EQ(
        Action(ActionType::NONE),
        manager()->EvaluateRequest(google_com_request, is_incognito_context));
    tester.ExpectTotalCount(kHistogramName, 0);
    example_com_request.dnr_action.reset();
    google_com_request.dnr_action.reset();
  }

  // Add an extension ruleset which blocks requests to "example.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  std::unique_ptr<CompositeMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule}, "test_extension", &matcher));
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                        URLPatternSet());

  {
    base::HistogramTester tester;
    EXPECT_EQ(
        Action(ActionType::BLOCK),
        manager()->EvaluateRequest(example_com_request, is_incognito_context));
    tester.ExpectTotalCount(kHistogramName, 1);
    EXPECT_EQ(
        Action(ActionType::NONE),
        manager()->EvaluateRequest(google_com_request, is_incognito_context));
    tester.ExpectTotalCount(kHistogramName, 2);
    example_com_request.dnr_action.reset();
    google_com_request.dnr_action.reset();
  }
}

// Test redirect rules.
TEST_P(RulesetManagerTest, Redirect) {
  // Add an extension ruleset which redirects "example.com" to "google.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("http://google.com");
  std::unique_ptr<CompositeMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule}, "test_extension", &matcher,
                            {"*://example.com/*", "*://abc.com/*"}));
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                        URLPatternSet());

  // Create a request to "example.com" with an empty initiator. It should be
  // redirected to "google.com".
  const bool is_incognito_context = false;
  const char* kExampleURL = "http://example.com";
  Action expected_redirect_action(ActionType::REDIRECT);
  expected_redirect_action.redirect_url = GURL("http://google.com");
  WebRequestInfo request_1(GetRequestParamsForURL(kExampleURL, base::nullopt));
  EXPECT_EQ(expected_redirect_action,
            manager()->EvaluateRequest(request_1, is_incognito_context));

  // Change the initiator to "xyz.com". It should not be redirected since we
  // don't have host permissions to the request initiator.
  WebRequestInfo request_2(GetRequestParamsForURL(
      kExampleURL, url::Origin::Create(GURL("http://xyz.com"))));
  EXPECT_EQ(Action(ActionType::NONE),
            manager()->EvaluateRequest(request_2, is_incognito_context));

  // Change the initiator to "abc.com". It should be redirected since we have
  // the required host permissions.
  WebRequestInfo request_3(GetRequestParamsForURL(
      kExampleURL, url::Origin::Create(GURL("http://abc.com"))));
  EXPECT_EQ(expected_redirect_action,
            manager()->EvaluateRequest(request_3, is_incognito_context));

  // Ensure web-socket requests are not redirected.
  WebRequestInfo request_4(
      GetRequestParamsForURL("ws://example.com", base::nullopt));
  EXPECT_EQ(Action(ActionType::NONE),
            manager()->EvaluateRequest(request_4, is_incognito_context));
}

// Tests that an extension can't block or redirect resources on the chrome-
// extension scheme.
TEST_P(RulesetManagerTest, ExtensionScheme) {
  const Extension* extension_1 = nullptr;
  const Extension* extension_2 = nullptr;
  // Add an extension with a background page which blocks all requests.
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension", &matcher,
        std::vector<std::string>({URLPattern::kAllUrlsPattern}),
        true /* has_background_script*/));
    extension_1 = last_loaded_extension();
    manager()->AddRuleset(extension_1->id(), std::move(matcher),
                          URLPatternSet());
  }

  // Add another extension with a background page which redirects all requests
  // to "http://google.com".
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    rule.priority = kMinValidPriority;
    rule.action->type = std::string("redirect");
    rule.action->redirect_url = std::string("http://google.com");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension_2", &matcher,
        std::vector<std::string>({URLPattern::kAllUrlsPattern}),
        true /* has_background_script*/));
    extension_2 = last_loaded_extension();
    manager()->AddRuleset(extension_2->id(), std::move(matcher),
                          URLPatternSet());
  }

  EXPECT_EQ(2u, manager()->GetMatcherCountForTest());

  // Ensure that "http://example.com" will be blocked (with blocking taking
  // priority over redirection).
  WebRequestInfo request_1(GetRequestParamsForURL("http://example.com"));
  EXPECT_EQ(
      Action(ActionType::BLOCK),
      manager()->EvaluateRequest(request_1, false /*is_incognito_context*/));

  // Ensure that the background page for |extension_1| won't be blocked or
  // redirected.
  GURL background_page_url_1 = BackgroundInfo::GetBackgroundURL(extension_1);
  EXPECT_TRUE(!background_page_url_1.is_empty());
  WebRequestInfo request_2(
      GetRequestParamsForURL(background_page_url_1.spec()));
  EXPECT_EQ(
      Action(ActionType::NONE),
      manager()->EvaluateRequest(request_2, false /*is_incognito_context*/));

  // Ensure that the background page for |extension_2| won't be blocked or
  // redirected.
  GURL background_page_url_2 = BackgroundInfo::GetBackgroundURL(extension_2);
  EXPECT_TRUE(!background_page_url_2.is_empty());
  WebRequestInfo request_3(
      GetRequestParamsForURL(background_page_url_2.spec()));
  EXPECT_EQ(
      Action(ActionType::NONE),
      manager()->EvaluateRequest(request_3, false /*is_incognito_context*/));

  // Also ensure that an arbitrary url on the chrome extension scheme is also
  // not blocked or redirected.
  WebRequestInfo request_4(GetRequestParamsForURL(base::StringPrintf(
      "%s://%s/%s", kExtensionScheme, "extension_id", "path")));
  EXPECT_EQ(
      Action(ActionType::NONE),
      manager()->EvaluateRequest(request_4, false /*is_incognito_context*/));
}

TEST_P(RulesetManagerTest, PageAllowingAPI) {
  // Add an extension which blocks all requests except for requests from
  // http://google.com/allow* which are allowed.
  {
    std::unique_ptr<CompositeMatcher> matcher;

    // This blocks all subresource requests. By default the main-frame resource
    // type is excluded.
    TestRule rule1 = CreateGenericRule();
    rule1.id = kMinValidID;
    rule1.condition->url_filter = std::string("*");

    // Also block main frame requests.
    TestRule rule2 = CreateGenericRule();
    rule2.id = kMinValidID + 1;
    rule2.condition->url_filter = std::string("*");
    rule2.condition->resource_types = std::vector<std::string>({"main_frame"});

    ASSERT_NO_FATAL_FAILURE(
        CreateMatcherForRules({rule1, rule2}, "test extension", &matcher, {},
                              true /* background_script */));

    URLPatternSet pattern_set(
        {URLPattern(URLPattern::SCHEME_ALL, "http://google.com/allow*")});
    manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                          std::move(pattern_set));
  }

  constexpr int kDummyFrameRoutingId = 2;
  constexpr int kDummyFrameId = 3;
  constexpr int kDummyParentFrameId = 1;
  constexpr int kDummyTabId = 5;
  constexpr int kDummyWindowId = 7;
  constexpr char kAllowedPageURL[] = "http://google.com/allow123";

  struct FrameDataParams {
    int frame_id;
    int parent_frame_id;
    std::string last_committed_main_frame_url;
    base::Optional<std::string> pending_main_frame_url;
  };
  struct TestCase {
    std::string url;
    content::ResourceType type;
    base::Optional<std::string> initiator;
    int frame_routing_id;
    base::Optional<FrameDataParams> frame_data_params;
    bool expect_blocked_with_allowed_pages;
  } test_cases[] = {
      // Main frame requests. Allowed based on request url.
      {kAllowedPageURL, content::ResourceType::kMainFrame, base::nullopt,
       MSG_ROUTING_NONE,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://google.com/xyz", base::nullopt}),
       false},
      {"http://google.com/xyz", content::ResourceType::kMainFrame,
       base::nullopt, MSG_ROUTING_NONE,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, base::nullopt}),
       true},

      // Non-navigation browser or service worker request. Not allowed,
      // since the request doesn't correspond to a frame.
      {"http://google.com/xyz", content::ResourceType::kScript, base::nullopt,
       MSG_ROUTING_NONE, base::nullopt, true},

      // Renderer requests - with no |pending_main_frame_url|. Allowed based
      // on the |last_committed_main_frame_url|.
      {kAllowedPageURL, content::ResourceType::kScript, "http://google.com",
       kDummyFrameRoutingId,
       FrameDataParams({kDummyFrameId, kDummyParentFrameId,
                        "http://google.com/xyz", base::nullopt}),
       true},
      {"http://google.com/xyz", content::ResourceType::kScript,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({kDummyFrameId, kDummyParentFrameId, kAllowedPageURL,
                        base::nullopt}),
       false},

      // Renderer requests with |pending_main_frame_url|. This only happens for
      // main frame subresource requests.

      // Here we'll determine "http://example.com/xyz" to be the main frame url
      // due to the origin.
      {"http://example.com/script.js", content::ResourceType::kScript,
       "http://example.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, "http://example.com/xyz"}),
       true},

      // Here we'll determine |kAllowedPageURL| to be the main
      // frame url due to the origin.
      {"http://example.com/script.js", content::ResourceType::kScript,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, "http://yahoo.com/xyz"}),
       false},

      // In these cases both |pending_main_frame_url| and
      // |last_committed_main_frame_url| will be tested since we won't be able
      // to determine the correct top level frame url using origin.
      {"http://example.com/script.js", content::ResourceType::kScript,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://google.com/abc", kAllowedPageURL}),
       false},
      {"http://example.com/script.js", content::ResourceType::kScript,
       base::nullopt, kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, "http://google.com/abc"}),
       false},
      {"http://example.com/script.js", content::ResourceType::kScript,
       base::nullopt, kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://yahoo.com/abc", "http://yahoo.com/allow123"}),
       true},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    const TestCase test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Testing case number %zu with url %s",
                                    i + 1, test_case.url.c_str()));

    WebRequestInfoInitParams params = GetRequestParamsForURL(test_case.url);
    ASSERT_TRUE(params.url.is_valid());
    params.type = test_case.type;

    if (test_case.initiator)
      params.initiator = url::Origin::Create(GURL(*test_case.initiator));

    params.frame_id = test_case.frame_routing_id;

    if (test_case.frame_data_params) {
      const FrameDataParams& frame_params = *test_case.frame_data_params;
      params.frame_data = ExtensionApiFrameIdMap::FrameData(
          frame_params.frame_id, frame_params.parent_frame_id, kDummyTabId,
          kDummyWindowId, GURL(frame_params.last_committed_main_frame_url));
      if (frame_params.pending_main_frame_url)
        params.frame_data->pending_main_frame_url =
            GURL(*frame_params.pending_main_frame_url);
    }

    Action expected_action = test_case.expect_blocked_with_allowed_pages
                                 ? Action(ActionType::BLOCK)
                                 : Action(ActionType::NONE);
    EXPECT_EQ(expected_action,
              manager()->EvaluateRequest(WebRequestInfo(std::move(params)),
                                         false /*is_incognito_context*/));
  }
}

TEST_P(RulesetManagerTest, HostPermissionForInitiator) {
  // Add an extension which redirects all sub-resource and sub-frame requests
  // made to example.com, to foo.com. By default, the "main_frame" type is
  // excluded if no "resource_types" are specified.
  std::unique_ptr<CompositeMatcher> redirect_matcher;
  {
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID;
    rule.priority = kMinValidPriority;
    rule.condition->url_filter = std::string("example.com");
    rule.action->type = std::string("redirect");
    rule.action->redirect_url = std::string("https://foo.com");
    std::vector<std::string> host_permissions = {"*://yahoo.com/*",
                                                 "*://example.com/*"};
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "redirecting extension", &redirect_matcher, host_permissions,
        false /* has_background_script */));
  }
  std::string redirect_extension_id = last_loaded_extension()->id();

  // Add an extension which blocks all sub-resource and sub-frame requests to
  // example.com. By default, the "main_frame" type is excluded if no
  // "resource_types" are specified. The extension has no host permissions.
  std::unique_ptr<CompositeMatcher> blocking_matcher;
  {
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID;
    rule.condition->url_filter = std::string("example.com");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "blocking extension", &blocking_matcher,
        {} /* host_permissions */, false /* has_background_script */));
  }
  std::string blocking_extension_id = last_loaded_extension()->id();

  struct TestCase {
    std::string url;
    base::Optional<url::Origin> initiator;
    ActionType expected_action_redirect_extension;
    ActionType expected_action_blocking_extension;
  } cases[] = {
      // Empty initiator. Has access.
      {"https://example.com", base::nullopt, ActionType::REDIRECT,
       ActionType::BLOCK},
      // Opaque origin as initiator. Has access.
      {"https://example.com", url::Origin(), ActionType::REDIRECT,
       ActionType::BLOCK},
      // yahoo.com as initiator. Has access.
      {"https://example.com", url::Origin::Create(GURL("http://yahoo.com")),
       ActionType::REDIRECT, ActionType::BLOCK},
      // No matching rule.
      {"https://yahoo.com", url::Origin::Create(GURL("http://example.com")),
       ActionType::NONE, ActionType::NONE},
      // Doesn't have access to initiator. But blocking a request doesn't
      // require host permissions.
      {"https://example.com", url::Origin::Create(GURL("http://google.com")),
       ActionType::NONE, ActionType::BLOCK},
  };

  auto verify_test_case = [this](const std::string& url,
                                 const base::Optional<url::Origin>& initiator,
                                 const Action& expected_action) {
    SCOPED_TRACE(base::StringPrintf(
        "Url-%s initiator-%s", url.c_str(),
        initiator ? initiator->Serialize().c_str() : "empty"));

    WebRequestInfo request(GetRequestParamsForURL(url, initiator));

    bool is_incognito_context = false;
    EXPECT_EQ(expected_action,
              manager()->EvaluateRequest(request, is_incognito_context));
  };

  // Test redirect extension.
  {
    SCOPED_TRACE("Testing redirect extension");
    manager()->AddRuleset(redirect_extension_id, std::move(redirect_matcher),
                          URLPatternSet());
    for (const auto& test : cases) {
      Action expected_action(test.expected_action_redirect_extension);
      if (test.expected_action_redirect_extension == ActionType::REDIRECT)
        expected_action.redirect_url = GURL("https://foo.com/");

      verify_test_case(test.url, test.initiator, expected_action);
    }
    manager()->RemoveRuleset(redirect_extension_id);
  }

  // Test blocking extension.
  {
    SCOPED_TRACE("Testing blocking extension");
    manager()->AddRuleset(blocking_extension_id, std::move(blocking_matcher),
                          URLPatternSet());
    for (const auto& test : cases) {
      verify_test_case(test.url, test.initiator,
                       Action(test.expected_action_blocking_extension));
    }
    manager()->RemoveRuleset(blocking_extension_id);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         RulesetManagerTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
