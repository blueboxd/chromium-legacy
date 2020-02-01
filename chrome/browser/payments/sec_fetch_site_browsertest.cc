// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {
namespace {

static constexpr char kPaymentMethod[] = "/";

class SecFetchSiteTest : public PlatformBrowserTest {
 public:
  SecFetchSiteTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    response_ = std::make_unique<net::test_server::ControllableHttpResponse>(
        &https_server_, kPaymentMethod);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_.Start());
    PlatformBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  GURL GetTestServerUrl(const std::string& hostname, const std::string& path) {
    return https_server_.GetURL(hostname, path);
  }

  std::string GetSecFetchSiteHeader() {
    response_->WaitForRequest();
    return response_->http_request()->headers.at("Sec-Fetch-Site");
  }

 private:
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> response_;
};

// When merchant https://a.com uses the payment method from https://b.com, the
// HTTP HEAD request has a "Sec-Fetch-Site: cross-site" header.
IN_PROC_BROWSER_TEST_F(SecFetchSiteTest,
                       CrossSitePaymentMethodManifestRequest) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      GetTestServerUrl("a.com", "/payment_request_creator.html")));
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("createPaymentRequest($1)",
                         GetTestServerUrl("b.com", kPaymentMethod).spec())));
  EXPECT_EQ("cross-site", GetSecFetchSiteHeader());
}

// When merchant https://a.com uses the payment method from https://a.com, the
// HTTP HEAD request has a "Sec-Fetch-Site: same-origin" header.
IN_PROC_BROWSER_TEST_F(SecFetchSiteTest,
                       SameOriginPaymentMethodManifestRequest) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      GetTestServerUrl("a.com", "/payment_request_creator.html")));
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("createPaymentRequest($1)",
                         GetTestServerUrl("a.com", kPaymentMethod).spec())));
  EXPECT_EQ("same-origin", GetSecFetchSiteHeader());
}

// When merchant https://x.a.com uses the payment method from https://y.a.com,
// the HTTP HEAD request has a "Sec-Fetch-Site: same-site" header.
IN_PROC_BROWSER_TEST_F(SecFetchSiteTest, SameSitePaymentMethodManifestRequest) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      GetTestServerUrl("x.a.com", "/payment_request_creator.html")));
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("createPaymentRequest($1)",
                         GetTestServerUrl("y.a.com", kPaymentMethod).spec())));
  EXPECT_EQ("same-site", GetSecFetchSiteHeader());
}

}  // namespace
}  // namespace payments
