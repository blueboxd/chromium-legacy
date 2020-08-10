// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/ancestor_throttle.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_navigation_url_loader.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using HeaderDisposition = AncestorThrottle::HeaderDisposition;

net::HttpResponseHeaders* GetAncestorHeaders(const char* xfo, const char* csp) {
  std::string header_string("HTTP/1.1 200 OK\nX-Frame-Options: ");
  header_string += xfo;
  if (csp != nullptr) {
    header_string += "\nContent-Security-Policy: ";
    header_string += csp;
  }
  header_string += "\n\n";
  std::replace(header_string.begin(), header_string.end(), '\n', '\0');
  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(header_string);
  EXPECT_TRUE(headers->HasHeader("X-Frame-Options"));
  if (csp != nullptr)
    EXPECT_TRUE(headers->HasHeader("Content-Security-Policy"));
  return headers;
}

network::mojom::ContentSecurityPolicyPtr ParsePolicy(
    const std::string& policy) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy", policy);
  std::vector<network::mojom::ContentSecurityPolicyPtr> policies;
  network::AddContentSecurityPolicyFromHeaders(
      *headers, GURL("https://example.com/"), &policies);
  return std::move(policies[0]);
}

}  // namespace

// AncestorThrottleTest
// -------------------------------------------------------------

class AncestorThrottleTest : public testing::Test {};

TEST_F(AncestorThrottleTest, ParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* value;
  } cases[] = {
      // Basic keywords
      {"DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL"},

      // Repeated keywords
      {"DENY,DENY", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL,ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL, ALLOWALL"},

      // Case-insensitive
      {"deNy", HeaderDisposition::DENY, "deNy"},
      {"sAmEorIgIn", HeaderDisposition::SAMEORIGIN, "sAmEorIgIn"},
      {"AlLOWaLL", HeaderDisposition::ALLOWALL, "AlLOWaLL"},

      // Trim whitespace
      {" DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {" ALLOWALL ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {"   DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN   ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"   ALLOWALL   ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {" DENY , DENY ", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,  SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL  ,ALLOWALL", HeaderDisposition::ALLOWALL,
       "ALLOWALL, ALLOWALL"},
  };

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseXFrameOptionsHeader(headers.get(), &header_value));
    EXPECT_EQ(test.value, header_value);
  }
}

TEST_F(AncestorThrottleTest, ErrorsParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* failure;
  } cases[] = {
      // Empty == Invalid.
      {"", HeaderDisposition::INVALID, ""},

      // Invalid
      {"INVALID", HeaderDisposition::INVALID, "INVALID"},
      {"INVALID DENY", HeaderDisposition::INVALID, "INVALID DENY"},
      {"DENY DENY", HeaderDisposition::INVALID, "DENY DENY"},
      {"DE NY", HeaderDisposition::INVALID, "DE NY"},

      // Conflicts
      {"INVALID,DENY", HeaderDisposition::CONFLICT, "INVALID, DENY"},
      {"DENY,ALLOWALL", HeaderDisposition::CONFLICT, "DENY, ALLOWALL"},
      {"SAMEORIGIN,DENY", HeaderDisposition::CONFLICT, "SAMEORIGIN, DENY"},
      {"ALLOWALL,SAMEORIGIN", HeaderDisposition::CONFLICT,
       "ALLOWALL, SAMEORIGIN"},
      {"DENY,  SAMEORIGIN", HeaderDisposition::CONFLICT, "DENY, SAMEORIGIN"}};

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseXFrameOptionsHeader(headers.get(), &header_value));
    EXPECT_EQ(test.failure, header_value);
  }
}

TEST_F(AncestorThrottleTest, AllowsBlanketEnforcementOfRequiredCSP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kOutOfBlinkCSPEE);

  struct TestCase {
    const char* name;
    const char* request_origin;
    const char* response_origin;
    const char* allow_csp_from;
    bool expected_result;
  } cases[] = {
      {
          "About scheme allows",
          "http://example.com",
          "about://me",
          nullptr,
          true,
      },
      {
          "File scheme allows",
          "http://example.com",
          "file://me",
          nullptr,
          true,
      },
      {
          "Data scheme allows",
          "http://example.com",
          "data://me",
          nullptr,
          true,
      },
      {
          "Filesystem scheme allows",
          "http://example.com",
          "filesystem://me",
          nullptr,
          true,
      },
      {
          "Blob scheme allows",
          "http://example.com",
          "blob://me",
          nullptr,
          true,
      },
      {
          "Same origin allows",
          "http://example.com",
          "http://example.com",
          nullptr,
          true,
      },
      {
          "Same origin allows independently of header",
          "http://example.com",
          "http://example.com",
          "http://not-example.com",
          true,
      },
      {
          "Different origin does not allow",
          "http://example.com",
          "http://not.example.com",
          nullptr,
          false,
      },
      {
          "Different origin with right header allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com",
          true,
      },
      {
          "Different origin with right header 2 allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com/",
          true,
      },
      {
          "Different origin with wrong header does not allow",
          "http://example.com",
          "http://not-example.com",
          "http://not-example.com",
          false,
      },
      {
          "Wildcard header allows",
          "http://example.com",
          "http://not-example.com",
          "*",
          true,
      },
      {
          "Malformed header does not allow",
          "http://example.com",
          "http://not-example.com",
          "*; http://example.com",
          false,
      },
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (test.allow_csp_from)
      headers->AddHeader("allow-csp-from", test.allow_csp_from);
    auto allow_csp_from = network::ParseAllowCSPFromHeader(*headers);

    bool actual = AncestorThrottle::AllowsBlanketEnforcementOfRequiredCSP(
        url::Origin::Create(GURL(test.request_origin)),
        GURL(test.response_origin), allow_csp_from);
    EXPECT_EQ(test.expected_result, actual);
  }
}

class AncestorThrottleNavigationTest
    : public content::RenderViewHostTestHarness {};

TEST_F(AncestorThrottleNavigationTest,
       WillStartRequestAddsSecRequiredCSPHeader) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kOutOfBlinkCSPEE);

  // Perform an initial navigation to set up everything.
  NavigateAndCommit(GURL("https://test.com"));

  // Create a frame tree with different 'csp' attributes according to the
  // following graph:
  //
  // FRAME NAME                    | 'csp' attribute
  // ------------------------------|-------------------------------------
  // main_frame                    | (none)
  //  ├─child_with_csp             | script-src 'none'
  //  │  ├─grandchild_same_csp     | script-src 'none'
  //  │  ├─grandchild_no_csp       | (none)
  //  │  │ └─grandgrandchild       | (none)
  //  │  ├─grandchild_invalid_csp  | report-to group
  //  │  └─grandchild_invalid_csp2 | script-src 'none'; invalid-directive
  //  └─sibling                    | (none)
  //
  // Test that the required CSP of every frame is computed/inherited correctly
  // and that the Sec-Required-CSP header is set.

  auto* main_frame = static_cast<TestRenderFrameHost*>(main_rfh());

  auto* child_with_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_frame)
          ->AppendChild("child_frame"));
  child_with_csp->frame_tree_node()->set_csp_attribute(
      ParsePolicy("script-src 'none'"));

  auto* grandchild_same_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_frame"));
  grandchild_same_csp->frame_tree_node()->set_csp_attribute(
      ParsePolicy("script-src 'none'"));

  auto* grandchild_no_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_frame"));

  auto* grandgrandchild = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(grandchild_no_csp)
          ->AppendChild("grandgrandchild_frame"));

  auto* grandchild_invalid_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_frame"));
  grandchild_invalid_csp->frame_tree_node()->set_csp_attribute(
      ParsePolicy("report-to group"));

  auto* grandchild_invalid_csp2 = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_frame"));
  grandchild_invalid_csp2->frame_tree_node()->set_csp_attribute(
      ParsePolicy("script-src 'none'; invalid-directive"));

  auto* sibling = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_frame)
          ->AppendChild("sibling_frame"));

  struct TestCase {
    const char* name;
    TestRenderFrameHost* frame;
    const char* expected_header;
  } cases[] = {
      {
          "Main frame does not set header",
          main_frame,
          nullptr,
      },
      {
          "Frame with 'csp' attribute sets correct header",
          child_with_csp,
          "script-src 'none'",
      },
      {
          "Child with same 'csp' attribute as parent frame sets correct header",
          grandchild_same_csp,
          "script-src 'none'",
      },
      {
          "Child without 'csp' attribute inherits from parent",
          grandchild_no_csp,
          "script-src 'none'",
      },
      {
          "Grandchild without 'csp' attribute inherits from grandparent"
          "header",
          grandgrandchild,
          "script-src 'none'",
      },
      {
          "Child with invalid 'csp' attribute inherits from parent",
          grandchild_invalid_csp,
          "script-src 'none'",
      },
      {
          "Child with invalid 'csp' attribute inherits from parent 2",
          grandchild_invalid_csp2,
          "script-src 'none'",
      },
      {
          "Frame without 'csp' attribute does not set header",
          sibling,
          nullptr,
      },
  };

  for (auto test : cases) {
    SCOPED_TRACE(test.name);
    std::unique_ptr<NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            GURL("https://www.foo.com/"), test.frame);
    simulator->Start();
    NavigationRequest* request =
        NavigationRequest::From(simulator->GetNavigationHandle());
    std::string header_value;
    bool found = request->GetRequestHeaders().GetHeader("sec-required-csp",
                                                        &header_value);
    if (test.expected_header) {
      EXPECT_TRUE(found);
      EXPECT_EQ(test.expected_header, header_value);
    } else {
      EXPECT_FALSE(found);
    }

    // Complete the navigation and store the required CSP in the
    // RenderFrameHostImpl so that the next tests can rely on this.
    // TODO(antoniosartori): Update the NavigationSimulatorImpl so that this is
    // done automatically on commit.
    TestNavigationURLLoader* url_loader =
        static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
    auto response = network::mojom::URLResponseHead::New();
    response->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    response->parsed_headers = network::mojom::ParsedHeaders::New();
    response->parsed_headers->allow_csp_from =
        network::mojom::AllowCSPFromHeaderValue::NewAllowStar(true);
    url_loader->CallOnResponseStarted(std::move(response));
    auto* new_required_csp = request->required_csp();
    if (new_required_csp)
      test.frame->required_csp_ = new_required_csp->Clone();
  }
}

}  // namespace content
