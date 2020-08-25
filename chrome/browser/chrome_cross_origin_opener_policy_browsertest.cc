// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"

#include "content/public/browser/web_contents.h"
// Cross-Origin-Opener-Policy is a web platform feature implemented by content/.
// However, since ContentBrowserClientImpl::LogWebFeatureForCurrentPage() is
// currently left blank in content/, it can't be tested from content/. So it is
// tested from chrome/ instead.
class ChromeCrossOriginOpenerPolicyBrowserTest : public InProcessBrowserTest {
 public:
  ChromeCrossOriginOpenerPolicyBrowserTest() {
    features_.InitWithFeatures(
        {
            // Enabled:
            network::features::kCrossOriginOpenerPolicy,
            network::features::kCrossOriginEmbedderPolicy,
            network::features::kCrossOriginOpenerPolicyReporting,
        },
        {});
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  void SetUpOnMainThread() final { host_resolver()->AddRule("*", "127.0.0.1"); }

  void SetUpCommandLine(base::CommandLine* command_line) final {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  base::test::ScopedFeatureList features_;
};

// Check the kCrossOriginOpenerPolicyReporting feature usage.
IN_PROC_BROWSER_TEST_F(ChromeCrossOriginOpenerPolicyBrowserTest,
                       ReportingUsage) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer http_server(net::EmbeddedTestServer::TYPE_HTTP);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  http_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(http_server.Start());

  int expected_count = 0;
  base::HistogramTester histogram;

  auto expect_histogram_increased_by = [&](int count) {
    expected_count += count;
    histogram.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting,
        expected_count);
  };

  // No header => 0 count.
  {
    GURL url = https_server.GetURL("a.com", "/title1.html");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
    expect_histogram_increased_by(0);
  }

  // COOP-Report-Only + HTTP => 0 count.
  {
    GURL url = http_server.GetURL("a.com",
                                  "/set-header?"
                                  "Cross-Origin-Opener-Policy-Report-Only: "
                                  "same-origin; report-to%3d\"a\"");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
    expect_histogram_increased_by(0);
  }

  // COOP-Report-Only + HTTPS => 1 count.
  {
    GURL url = https_server.GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy-Report-Only: "
                                   "same-origin; report-to%3d\"a\"");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
    expect_histogram_increased_by(1);
  }

  // COOP + HTPS => 1 count.
  {
    GURL url = https_server.GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy: "
                                   "same-origin; report-to%3d\"a\"");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
    expect_histogram_increased_by(1);
  }

  // COOP + COOP-RO  + HTTPS => 1 count.
  {
    GURL url = https_server.GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy: "
                                   "same-origin; report-to%3d\"a\"&"
                                   "Cross-Origin-Opener-Policy-Report-Only: "
                                   "same-origin; report-to%3d\"a\"");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
    expect_histogram_increased_by(1);
  }
  // No report endpoints defined => 0 count.
  {
    GURL url = https_server.GetURL(
        "a.com",
        "/set-header?"
        "Cross-Origin-Opener-Policy: same-origin&"
        "Cross-Origin-Opener-Policy-Report-Only: same-origin");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
    expect_histogram_increased_by(0);
  }

  // Main frame (COOP-RO), subframe (COOP-RO) => 1 count.
  {
    GURL url = https_server.GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy-Report-Only: "
                                   "same-origin; report-to%3d\"a\"");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
    EXPECT_TRUE(content::ExecJs(web_contents(), content::JsReplace(R"(
      new Promise(resolve => {
        let iframe = document.createElement("iframe");
        iframe.src = $1;
        iframe.onload = resolve;
        document.body.appendChild(iframe);
      });
    )",
                                                                   url)));
    expect_histogram_increased_by(1);
  }

  // Main frame (no-headers), subframe (COOP-RO) => 0 count.
  {
    GURL main_document_url = https_server.GetURL("a.com", "/title1.html");
    GURL sub_document_url =
        https_server.GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Opener-Policy-Report-Only: "
                            "same-origin; report-to%3d\"a\"");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
    EXPECT_TRUE(
        content::ExecJs(web_contents(), content::JsReplace(R"(
      new Promise(resolve => {
        let iframe = document.createElement("iframe");
        iframe.src = $1;
        iframe.onload = resolve;
        document.body.appendChild(iframe);
      });
    )",
                                                           sub_document_url)));
    expect_histogram_increased_by(0);
  }
}

// TODO(arthursonzogni): Add basic test(s) for the WebFeatures:
// - CrossOriginOpenerPolicySameOrigin
// - CrossOriginOpenerPolicySameOriginAllowPopups
// - CrossOriginEmbedderPolicyRequireCorp
// - CoopAndCoepIsolated
//
// Added by:
// https://chromium-review.googlesource.com/c/chromium/src/+/2122140
//
// In particular, it would be interesting knowing what happens with iframes?
// Are CoopCoepOriginIsolated nested document counted as CoopAndCoepIsolated?
// Not doing it would underestimate the usage metric.
