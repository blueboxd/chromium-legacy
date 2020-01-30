// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/stl_util.h"
#include "content/common/content_security_policy/csp_context.h"
#include "content/common/navigation_params.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using CSPDirectiveName = network::mojom::CSPDirectiveName;

namespace {
class CSPContextTest : public CSPContext {
 public:
  CSPContextTest() : CSPContext() {}

  const std::vector<CSPViolationParams>& violations() { return violations_; }

  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.push_back(scheme);
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return base::Contains(scheme_to_bypass_, scheme);
  }

 private:
  void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params) override {
    violations_.push_back(violation_params);
  }
  std::vector<CSPViolationParams> violations_;
  std::vector<std::string> scheme_to_bypass_;

  DISALLOW_COPY_AND_ASSIGN(CSPContextTest);
};

network::mojom::ContentSecurityPolicyPtr EmptyCSP() {
  auto policy = network::mojom::ContentSecurityPolicy::New();
  policy->header = network::mojom::ContentSecurityPolicyHeader::New();
  return policy;
}

// Build a new policy made of only one directive and no report endpoints.
network::mojom::ContentSecurityPolicyPtr BuildPolicy(
    CSPDirectiveName directive_name,
    network::mojom::CSPSourcePtr source) {
  auto directive = network::mojom::CSPDirective::New();
  directive->name = directive_name;
  directive->source_list = network::mojom::CSPSourceList::New();
  directive->source_list->sources.push_back(std::move(source));
  auto policy = EmptyCSP();
  policy->directives.push_back(std::move(directive));
  return policy;
}

network::mojom::CSPSourcePtr BuildCSPSource(const char* scheme,
                                            const char* host) {
  return network::mojom::CSPSource::New(scheme, host, url::PORT_UNSPECIFIED, "",
                                        false, false);
}

// Return "Content-Security-Policy: default-src <host>"
network::mojom::ContentSecurityPolicyPtr DefaultSrc(const char* scheme,
                                                    const char* host) {
  return BuildPolicy(CSPDirectiveName::DefaultSrc,
                     BuildCSPSource(scheme, host));
}

}  // namespace

TEST(ContentSecurityPolicy, NoDirective) {
  CSPContextTest context;

  EXPECT_TRUE(CheckContentSecurityPolicy(
      EmptyCSP(), CSPDirectiveName::FormAction, GURL("http://www.example.com"),
      false, false, &context, SourceLocation(), true));
  ASSERT_EQ(0u, context.violations().size());
}

TEST(ContentSecurityPolicy, ReportViolation) {
  CSPContextTest context;
  auto policy = BuildPolicy(CSPDirectiveName::FormAction,
                            BuildCSPSource("", "www.example.com"));

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FormAction, GURL("http://www.not-example.com"),
      false, false, &context, SourceLocation(), true));

  ASSERT_EQ(1u, context.violations().size());
  const char console_message[] =
      "Refused to send form data to 'http://www.not-example.com/' because it "
      "violates the following Content Security Policy directive: \"form-action "
      "www.example.com\".\n";
  EXPECT_EQ(console_message, context.violations()[0].console_message);
}

TEST(ContentSecurityPolicy, DirectiveFallback) {
  auto allow_host = [](const char* host) {
    std::vector<network::mojom::CSPSourcePtr> sources;
    sources.push_back(BuildCSPSource("http", host));
    return network::mojom::CSPSourceList::New(std::move(sources), false, false,
                                              false);
  };

  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives.push_back(network::mojom::CSPDirective::New(
        CSPDirectiveName::DefaultSrc, allow_host("a.com")));
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), false, false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"default-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'default-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), false, false,
                                           &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives.push_back(network::mojom::CSPDirective::New(
        CSPDirectiveName::ChildSrc, allow_host("a.com")));
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), false, false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"child-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'child-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), false, false,
                                           &context, SourceLocation(), false));
  }
  {
    CSPContextTest context;
    auto policy = EmptyCSP();
    policy->directives.push_back(network::mojom::CSPDirective::New(
        CSPDirectiveName::FrameSrc, allow_host("a.com")));
    policy->directives.push_back(network::mojom::CSPDirective::New(
        CSPDirectiveName::ChildSrc, allow_host("b.com")));
    EXPECT_TRUE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                           GURL("http://a.com"), false, false,
                                           &context, SourceLocation(), false));
    EXPECT_FALSE(CheckContentSecurityPolicy(policy, CSPDirectiveName::FrameSrc,
                                            GURL("http://b.com"), false, false,
                                            &context, SourceLocation(), false));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"frame-src "
        "http://a.com\".\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
  }
}

TEST(ContentSecurityPolicy, RequestsAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      false, false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      false, false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, RequestsAllowedWhenHostMixedCase) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "ExAmPle.com");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://example.com/"), false,
      false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("https://not-example.com/"),
      false, false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, FilesystemAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, false, &context,
      SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, false,
      &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, false, &context,
      SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, false,
      &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, BlobAllowedWhenBypassingCSP) {
  CSPContextTest context;
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://example.com/"),
      false, false, &context, SourceLocation(), false));
  EXPECT_FALSE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://not-example.com/"),
      false, false, &context, SourceLocation(), false));

  // Register 'https' as bypassing CSP, which should now bypass it entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://example.com/"),
      false, false, &context, SourceLocation(), false));
  EXPECT_TRUE(CheckContentSecurityPolicy(
      policy, CSPDirectiveName::FrameSrc, GURL("blob:https://not-example.com/"),
      false, false, &context, SourceLocation(), false));
}

TEST(ContentSecurityPolicy, ShouldUpgradeInsecureRequest) {
  auto policy = DefaultSrc("https", "example.com");

  EXPECT_FALSE(ShouldUpgradeInsecureRequest(policy));

  policy->directives.push_back(network::mojom::CSPDirective::New(
      CSPDirectiveName::UpgradeInsecureRequests,
      network::mojom::CSPSourceList::New()));
  EXPECT_TRUE(ShouldUpgradeInsecureRequest(policy));
}

TEST(ContentSecurityPolicy, NavigateToChecks) {
  GURL url_a("https://a");
  GURL url_b("https://b");
  CSPContextTest context;
  auto allow_none = [] {
    return network::mojom::CSPSourceList::New(
        std::vector<network::mojom::CSPSourcePtr>(), false, false, false);
  };
  auto allow_self = [] {
    return network::mojom::CSPSourceList::New(
        std::vector<network::mojom::CSPSourcePtr>(), true, false, false);
  };
  auto allow_redirect = [] {
    return network::mojom::CSPSourceList::New(
        std::vector<network::mojom::CSPSourcePtr>(), false, false, true);
  };
  auto source_a = [] {
    return network::mojom::CSPSource::New("https", "a", url::PORT_UNSPECIFIED,
                                          "", false, false);
  };
  auto allow_a = [&] {
    std::vector<network::mojom::CSPSourcePtr> sources;
    sources.push_back(source_a());
    return network::mojom::CSPSourceList::New(std::move(sources), false, false,
                                              false);
  };
  auto allow_redirect_a = [&] {
    std::vector<network::mojom::CSPSourcePtr> sources;
    sources.push_back(source_a());
    return network::mojom::CSPSourceList::New(std::move(sources), false, false,
                                              true);
  };
  context.SetSelf(source_a());

  struct TestCase {
    network::mojom::CSPSourceListPtr navigate_to_list;
    const GURL& url;
    bool is_response_check;
    bool is_form_submission;
    network::mojom::CSPSourceListPtr form_action_list;
    bool expected;
  } cases[] = {
      // Basic source matching.
      {allow_none(), url_a, false, false, {}, false},
      {allow_a(), url_a, false, false, {}, true},
      {allow_a(), url_b, false, false, {}, false},
      {allow_self(), url_a, false, false, {}, true},

      // Checking allow_redirect flag interactions.
      {allow_redirect(), url_a, false, false, {}, true},
      {allow_redirect(), url_a, true, false, {}, false},
      {allow_redirect_a(), url_a, false, false, {}, true},
      {allow_redirect_a(), url_a, true, false, {}, true},

      // Interaction with form-action:

      // Form submission without form-action present.
      {allow_none(), url_a, false, true, {}, false},
      {allow_a(), url_a, false, true, {}, true},
      {allow_a(), url_b, false, true, {}, false},
      {allow_self(), url_a, false, true, {}, true},

      // Form submission with form-action present.
      {allow_none(), url_a, false, true, allow_a(), true},
      {allow_a(), url_a, false, true, allow_a(), true},
      {allow_a(), url_b, false, true, allow_a(), true},
      {allow_self(), url_a, false, true, allow_a(), true},
  };

  for (auto& test : cases) {
    auto policy = EmptyCSP();
    policy->directives.push_back(network::mojom::CSPDirective::New(
        CSPDirectiveName::NavigateTo, std::move(test.navigate_to_list)));

    if (test.form_action_list) {
      policy->directives.push_back(network::mojom::CSPDirective::New(
          CSPDirectiveName::FormAction, std::move(test.form_action_list)));
    }

    EXPECT_EQ(test.expected, CheckContentSecurityPolicy(
                                 policy, CSPDirectiveName::NavigateTo, test.url,
                                 true, test.is_response_check, &context,
                                 SourceLocation(), test.is_form_submission));
    EXPECT_EQ(test.expected, CheckContentSecurityPolicy(
                                 policy, CSPDirectiveName::NavigateTo, test.url,
                                 false, test.is_response_check, &context,
                                 SourceLocation(), test.is_form_submission));
  }
}
}  // namespace content
