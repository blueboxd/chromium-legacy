// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

class SubresourceRedirectBrowserTest : public InProcessBrowserTest {
 public:
  SubresourceRedirectBrowserTest() {}

  void SetUp() override {
    // |http_server| setup.
    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory("chrome/test/data");

    ASSERT_TRUE(http_server_->Start());

    http_url_ = http_server_->GetURL("insecure.com", "/");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    // |https_server| setup.
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");

    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("secure.com", "/");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    // |compression_server| setup.
    compression_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    compression_server_->RegisterRequestHandler(base::BindRepeating(
        &SubresourceRedirectBrowserTest::HandleCompressionServerRequest,
        base::Unretained(this)));

    ASSERT_TRUE(compression_server_->Start());

    compression_url_ = compression_server_->GetURL("compression.com", "/");
    ASSERT_TRUE(compression_url_.SchemeIs(url::kHttpsScheme));

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("litepage-server-subresource-host",
                                    compression_url_.spec());

    command_line->AppendSwitch("enable-subresource-redirect");

    //  Need to resolve all 3 of the above servers to 127.0.0.1:port, and
    //  the servers themselves can't serve using 127.0.0.1:port as the
    //  compressed resource URLs rely on subdomains, and subdomains
    //  do not function properly when using 127.0.0.1:port
    command_line->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
  }

  bool RunScriptExtractBool(const std::string& script) {
    bool result;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(), script, &result));
    return result;
  }

  GURL http_url() const { return http_url_; }
  GURL https_url() const { return https_url_; }
  GURL compression_url() const { return compression_url_; }

  GURL HttpURLWithPath(const std::string& path) {
    return http_server_->GetURL("insecure.com", path);
  }
  GURL HttpsURLWithPath(const std::string& path) {
    return https_server_->GetURL("secure.com", path);
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Called by |compression_server_|.
  std::unique_ptr<net::test_server::HttpResponse>
  HandleCompressionServerRequest(const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    // For the purpose of this browsertest, a redirect to the compression server
    // that is looking to access image.png will be treated as though it is
    // compressed.  All other redirects will be assumed failures to retrieve the
    // requested resource and return a redirect to private_url_image.png.
    if (request.GetURL().query().find(
            net::EscapeQueryParamValue("/image.png", true /* use_plus */), 0) !=
        std::string::npos) {
      response->set_code(net::HTTP_OK);
    } else {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader(
          "Location",
          HttpsURLWithPath("/load_image/private_url_image.png").spec());
    }
    return std::move(response);
  }

  GURL compression_url_;
  GURL http_url_;
  GURL https_url_;

  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> compression_server_;

  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceRedirectBrowserTest);
};

//  NOTE: It is indirectly verified that correct requests are being sent to
//  the mock compression server by the counts in the histogram bucket for
//  HTTP_TEMPORARY_REDIRECTs.

//  This test loads image.html, which triggers a subresource request
//  for image.png.  This triggers an internal redirect to the mocked
//  compression server, which responds with HTTP_OK.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestHTMLLoadRedirectSuccess) {
  base::RunLoop().RunUntilIdle();
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/image.html"));

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.Status", 2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.Status", net::HTTP_OK, 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.Status",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
}

//  This test loads private_url_image.html, which triggers a subresource
//  request for private_url_image.png.  This triggers an internal redirect
//  to the mock compression server, which bypasses the request. The
//  mock compression server creates a redirect to the original resource.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestHTMLLoadRedirectBypass) {
  ui_test_utils::NavigateToURL(
      browser(), HttpsURLWithPath("/load_image/private_url_image.html"));

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.Status", 2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.Status",
      net::HTTP_TEMPORARY_REDIRECT, 2);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
}

//  This test loads image_js.html, which triggers a javascript request
//  for image.png.  This triggers an internal redirect to the mocked
//  compression server, which responds with HTTP_OK.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestJavaScriptRedirectSuccess) {
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/image_js.html"));

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.Status", 2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.Status", net::HTTP_OK, 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.Status",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
}

//  This test loads private_url_image.html, which triggers a javascript
//  request for private_url_image.png.  This triggers an internal redirect
//  to the mock compression server, which bypasses the request. The
//  mock compression server creates a redirect to the original resource.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestJavaScriptRedirectBypass) {
  ui_test_utils::NavigateToURL(
      browser(), HttpsURLWithPath("/load_image/private_url_image_js.html"));

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.Status", 2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.Status",
      net::HTTP_TEMPORARY_REDIRECT, 2);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
}

//  This test loads image.html, from a non secure site. This triggers a
//  subresource request, but no internal redirect should be created for
//  non-secure sites.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       NoTriggerOnNonSecureSite) {
  ui_test_utils::NavigateToURL(browser(),
                               HttpURLWithPath("/load_image/image.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.Status", 0);
}

//  This test loads page_with_favicon.html, which creates a subresource
//  request for icon.png.  There should be no internal redirect as favicons
//  are not considered images by chrome.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest, NoTriggerOnNonImage) {
  ui_test_utils::NavigateToURL(
      browser(), HttpsURLWithPath("/favicon/page_with_favicon.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.Status", 0);
}

}  // namespace
