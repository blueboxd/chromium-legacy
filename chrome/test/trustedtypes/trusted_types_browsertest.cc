// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {
// Origin Trial tokens are bound to a specific origin (incl. port), so we need
// to force our test server to run on the same port that the test token has
// been generated for.
const int kServerPort = 54321;

// We (thankfully) cannot generate origin trial tokens with the production key
// There is an origin trial 'test key' is documented of sorts, but does not
// have a standard API.
// Ref: docs/origin_trials_integration.md
// Ref: src/third_party/blink/common/origin_trials/trial_token_unittest.cc
constexpr char kOriginTrialTestPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

// Origin Trial Token for TrustedDOMTypes generated with:
// $ tools/origin_trials/generate_token.py  \
//     https://127.0.0.1:54321/  \
//     "TrustedDOMTypes"  \
//     --expire-timestamp=2000000000
// (Token will expire ca. ~2033. See content/test/data/origin_trials/basic.html)
constexpr char kOriginTrialToken[] =
    "AnRnI2yGt1XQTaKUvbAQ8nRas1bXSDIWwfjeEaDKtXvHgid7wigd4IMm4DkBWsFWM+"
    "Cww0rgYOpQpBWPBPN8xQwAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NTQzMjEi"
    "LCAiZmVhdHVyZSI6ICJUcnVzdGVkRE9NVHlwZXMiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";
constexpr char kTrustedTypesCSP[] = "trusted-types *";
constexpr char kDefaultResponseTemplate[] = R"(
<html>
<head>
  <title>(starting)</title>
  META
  <script id="target"></script>
  <script>
    const tt_available = window.TrustedTypes ? "enabled" : "disabled";
    const target = document.getElementById("target");
    let tt_enforced = "dont know yet";
    try {
      target.textContent = "2+2;";
      tt_enforced = "ignored";
    } catch (e) {
      tt_enforced = "enforced";
    }
    document.title = `Trusted Types: ${tt_available} and ${tt_enforced}`;
  </script>
</head>
<body>
  <p>Hello World!</p>
  <p>This test sets the document title.</p>
</body>
</html>
)";

// The expected test page titles when Trusted Types are disabled or enabled:
constexpr char kTitleEnabled[] = "Trusted Types: enabled and enforced";
constexpr char kTitleDisabled[] = "Trusted Types: disabled and ignored";
constexpr char kTitleAvailable[] = "Trusted Types: enabled and ignored";

// Generate a test response, based on kDefaultResponseTemplate.
// If the request path contains:
// - "otheader"/"otmeta": Put Origin Trial in header/<meta> element.
//   (Use the token from kOriginTrialToken.)
// - "cspheader"/"cspmeta": Put CSP into header/<meta> element.
//   (Use the CSP from kTrustedTypesCSP.
// Return 404 for all paths not ending in ".html".
std::unique_ptr<net::test_server::HttpResponse> TrustedTypesTestHandler(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  std::string url = request.GetURL().spec();
  if (!base::EndsWith(url, ".html", base::CompareCase::SENSITIVE)) {
    response->set_code(net::HTTP_NOT_FOUND);
    return response;
  }

  std::string meta;
  if (url.find("otheader") != std::string::npos) {
    response->AddCustomHeader("origin-trial", kOriginTrialToken);
  } else if (url.find("otmeta") != std::string::npos) {
    meta.append(std::string() + R"(<meta http-equiv="origin-trial" content=")" +
                kOriginTrialToken + R"(">)");
  }
  if (url.find("cspheader") != std::string::npos) {
    response->AddCustomHeader("Content-Security-Policy", kTrustedTypesCSP);
  } else if (url.find("cspmeta") != std::string::npos) {
    meta.append(std::string() +
                R"(<meta http-equiv="Content-Security-Policy" content=")" +
                kTrustedTypesCSP + R"(">)");
  }

  std::string contents = kDefaultResponseTemplate;
  base::ReplaceFirstSubstringAfterOffset(&contents, 0, "META", meta);
  response->set_content(contents);
  response->set_content_type("text/html");
  response->set_code(net::HTTP_OK);
  return response;
}

}  // namespace

// TrustedTypesBrowserTest tests activation of Trusted Types via CSP and Origin
// Trial. (The tests for the actual TT functionality are found in
// external/wpt/trusted-types/*.)
class TrustedTypesBrowserTest : public InProcessBrowserTest {
 public:
  TrustedTypesBrowserTest() = default;
  ~TrustedTypesBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    server_->RegisterRequestHandler(base::Bind(&TrustedTypesTestHandler));
    EXPECT_TRUE(server()->Start(kServerPort));
  }

  void TearDownInProcessBrowserTestFixture() override { server_.reset(); }

  net::test_server::EmbeddedTestServer* server() { return server_.get(); }

  base::string16 NavigateToAndReturnTitle(const char* url) {
    EXPECT_TRUE(server());
    ui_test_utils::NavigateToURL(browser(), GURL(server()->GetURL(url)));
    base::string16 title;
    ui_test_utils::GetCurrentTabTitle(browser(), &title);
    return title;
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

  void SetUp() override {
    // We need to explicitly disable the feature, so that our test cases can
    // verify whether enabling it actually works.
    feature_list_.InitAndDisableFeature(features::kTrustedDOMTypes);
    InProcessBrowserTest::SetUp();
  }

 private:
  std::unique_ptr<net::test_server::EmbeddedTestServer> server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(TrustedTypesBrowserTest);
};

// Our test cases are effectively a 3x3 matrix of origin trial token (absent|in
// header|in <meta>) and content security policy (absent|in header|in <meta>).
// The test fixture will generate the appropriate page based on the URL path.

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest, PagePlainX) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleDisabled),
            NavigateToAndReturnTitle("/page.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest, PageWithTokenInHeader) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleAvailable),
            NavigateToAndReturnTitle("/page-otheader.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest, PageWithTokenInMeta) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleAvailable),
            NavigateToAndReturnTitle("/page-otmeta.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest, PageWithCSPInHeaderX) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleDisabled),
            NavigateToAndReturnTitle("/page-cspheader.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest, PageWithCSPAndTokenInHeader) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleEnabled),
            NavigateToAndReturnTitle("/page-cspheader-otheader.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest,
                       PageWithCSPInHeaderAndTokenInMeta) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleEnabled),
            NavigateToAndReturnTitle("/page-cspheader-otmeta.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest, PageWithCSPInMetaX) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleDisabled),
            NavigateToAndReturnTitle("/page-cspmeta.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest,
                       PageWithCSPInMetaAndTokenInHeader) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleEnabled),
            NavigateToAndReturnTitle("/page-cspmeta-otheader.html"));
}

IN_PROC_BROWSER_TEST_F(TrustedTypesBrowserTest, PageWithCSPAndTokenInMeta) {
  EXPECT_EQ(base::ASCIIToUTF16(kTitleEnabled),
            NavigateToAndReturnTitle("/page-cspmeta-otmeta.html"));
}
