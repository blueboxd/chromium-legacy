// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/base32/base32.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/public_image_metadata.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
// TODO(rajendrant): Convert the tests to wait for image load to complete or the
// page load complete, instead of waiting on the histograms.
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
  explicit SubresourceRedirectBrowserTest(
      bool enable_subresource_server_redirect = true)
      : enable_subresource_server_redirect_(enable_subresource_server_redirect),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        compression_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    // |http_server| setup.
    http_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(http_server_.Start());
    http_url_ = http_server_.GetURL("insecure.com", "/");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    // |https_server| setup.
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &SubresourceRedirectBrowserTest::HandleHTTPSServerRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());
    https_url_ = https_server_.GetURL("secure.com", "/");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    // |compression_server| setup.
    compression_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    compression_server_.RegisterRequestHandler(base::BindRepeating(
        &SubresourceRedirectBrowserTest::HandleCompressionServerRequest,
        base::Unretained(this)));
    ASSERT_TRUE(compression_server_.Start());
    compression_url_ = compression_server_.GetURL("compression.com", "/");
    ASSERT_TRUE(compression_url_.SchemeIs(url::kHttpsScheme));

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect,
          {{"enable_subresource_server_redirect",
            enable_subresource_server_redirect_ ? "true" : "false"},
           {"lite_page_subresource_origin", compression_url_.spec()}}},
         {optimization_guide::features::kOptimizationHints, {}},
         {optimization_guide::features::kRemoteOptimizationGuideFetching, {}}},
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Need to resolve all 3 of the above servers to 127.0.0.1:port, and
    // the servers themselves can't serve using 127.0.0.1:port as the
    // compressed resource URLs rely on subdomains, and subdomains
    // do not function properly when using 127.0.0.1:port
    command_line->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    command_line->AppendSwitch("enable-spdy-proxy-auth");
    command_line->AppendSwitch("optimization-guide-disable-installer");
    command_line->AppendSwitch("purge_hint_cache_store");
  }

  void EnableDataSaver(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(),
                                      enabled);
    base::RunLoop().RunUntilIdle();
  }

  bool RunScriptExtractBool(const std::string& script,
                            content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return EvalJs(web_contents, script).ExtractBool();
  }

  std::string RunScriptExtractString(
      const std::string& script,
      content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(web_contents, script, &result));
    return result;
  }

  // Sets up public image URL hint data.
  void SetUpPublicImageURLPaths(
      const GURL& url,
      const std::vector<std::string>& public_image_paths) {
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide::proto::PublicImageMetadata public_image_metadata;
    for (const auto& image_path : public_image_paths) {
      public_image_metadata.add_url(
          https_server_.GetURL("secure.com", image_path).spec());
    }

    optimization_guide::OptimizationMetadata optimization_metadata;
    optimization_metadata.set_public_image_metadata(public_image_metadata);
    optimization_guide_decider->AddHintForTesting(
        url, optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
        optimization_metadata);
  }

  void CreateUkmRecorder() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  std::map<uint64_t, size_t> GetImageCompressionUkmMetrics() {
    using ImageCompressionUkm = ukm::builders::PublicImageCompressionDataUse;
    std::map<uint64_t, size_t> metric_bytes;
    // Flatten the metrics from multiple ukm sources.
    for (const auto* metrics :
         ukm_recorder_->GetEntriesByName(ImageCompressionUkm::kEntryName)) {
      for (const auto& metric : metrics->metrics) {
        if (metric_bytes.find(metric.first) == metric_bytes.end())
          metric_bytes[metric.first] = 0;
        metric_bytes[metric.first] += metric.second;
      }
    }
    return metric_bytes;
  }

  void WaitForImageCompressionUkmMetrics(size_t count) {
    while (ukm_recorder_
               ->GetEntriesByName(
                   ukm::builders::PublicImageCompressionDataUse::kEntryName)
               .size() < count) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void VerifyPublicImageCompressionUkm(uint64_t hash, size_t num_images) {
    const auto metrics = GetImageCompressionUkmMetrics();
    if (num_images) {
      EXPECT_THAT(metrics, testing::Contains(
                               testing::Pair(hash,
                                             testing::Gt(num_images * 500))));
    } else {
      EXPECT_EQ(metrics.find(hash), metrics.end());
    }
  }

  void VerifyCompressibleImageUkm(size_t num_images) {
    VerifyPublicImageCompressionUkm(
        ukm::builders::PublicImageCompressionDataUse::
            kCompressibleImageBytesNameHash,
        num_images);
  }

  void VerifyIneligibleImageHintsUnavailableUkm(size_t num_images) {
    VerifyPublicImageCompressionUkm(
        ukm::builders::PublicImageCompressionDataUse::
            kIneligibleImageHintsUnavailableBytesNameHash,
        num_images);
  }

  void VerifyIneligibleImageHintsUnavailableUkmButCompressible(
      size_t num_images) {
    VerifyPublicImageCompressionUkm(
        ukm::builders::PublicImageCompressionDataUse::
            kIneligibleImageHintsUnavailableButCompressibleBytesNameHash,
        num_images);
  }

  void VerifyIneligibleImageHintsUnavailableAndMissingInHintsUkm(
      size_t num_images) {
    VerifyPublicImageCompressionUkm(
        ukm::builders::PublicImageCompressionDataUse::
            kIneligibleImageHintsUnavailableAndMissingInHintsBytesNameHash,
        num_images);
  }

  void VerifyIneligibleMissingInImageHintsUkm(size_t num_images) {
    VerifyPublicImageCompressionUkm(
        ukm::builders::PublicImageCompressionDataUse::
            kIneligibleMissingInImageHintsBytesNameHash,
        num_images);
  }

  void VerifyIneligibleOtherImageUkm(size_t num_images) {
    VerifyPublicImageCompressionUkm(
        ukm::builders::PublicImageCompressionDataUse::
            kIneligibleOtherImageBytesNameHash,
        num_images);
  }

  GURL GetSubresourceURLForURL(const std::string& path) {
    GURL compressed_url = compression_url();
    std::string origin_hash = base::ToLowerASCII(base32::Base32Encode(
        crypto::SHA256HashString(
            https_url().scheme() + "://" + https_url().host() + ":" +
            base::NumberToString(https_url().EffectiveIntPort())),
        base32::Base32EncodePolicy::OMIT_PADDING));
    std::string host_str = origin_hash + "." + compressed_url.host();
    GURL::Replacements replacements;
    replacements.SetHostStr(host_str);
    replacements.SetPathStr(path);
    return compressed_url.ReplaceComponents(replacements);
  }

  GURL http_url() const { return http_url_; }
  GURL https_url() const { return https_url_; }
  GURL compression_url() const { return compression_url_; }
  GURL request_url() const { return request_url_; }

  GURL HttpURLWithPath(const std::string& path) {
    return http_server_.GetURL("insecure.com", path);
  }
  GURL HttpsURLWithPath(const std::string& path) {
    return https_server_.GetURL("secure.com", path);
  }

  void SetHttpsServerImageToFail() { https_server_image_fail_ = true; }
  void SetCompressionServerToFail() { compression_server_fail_ = true; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Handles the https server request.
  std::unique_ptr<net::test_server::HttpResponse> HandleHTTPSServerRequest(
      const net::test_server::HttpRequest& request) {
    if (https_server_image_fail_ &&
        request.GetURL().path() == "/load_image/image.png") {
      return std::make_unique<net::test_server::RawHttpResponse>("", "");
    }
    return nullptr;
  }

  // Handles the compression server request.
  std::unique_ptr<net::test_server::HttpResponse>
  HandleCompressionServerRequest(const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    request_url_ = request.GetURL();

    // If |compression_server_fail_| is set to true, return a hung response.
    if (compression_server_fail_ == true) {
      return std::make_unique<net::test_server::RawHttpResponse>("", "");
    }

    // For the purpose of this browsertest, a redirect to the compression server
    // that is looking to access image.png will be treated as though it is
    // compressed.  All other redirects will be assumed failures to retrieve the
    // requested resource and return a redirect to private_url_image.png.
    if (request_url_.query().find(
            net::EscapeQueryParamValue("/image.png", true /* use_plus */), 0) !=
        std::string::npos) {
      // Serve the correct image file.
      std::string file_contents;
      base::FilePath test_data_directory;
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
      if (base::ReadFileToString(
              test_data_directory.AppendASCII("load_image/image.png"),
              &file_contents)) {
        response->set_content(file_contents);
        response->set_code(net::HTTP_OK);
      }
    } else if (request_url_.query().find(
                   net::EscapeQueryParamValue("/fail_image.png",
                                              true /* use_plus */),
                   0) != std::string::npos) {
      response->set_code(net::HTTP_NOT_FOUND);
    } else if (base::StartsWith(request_url_.path(), "/load_image/",
                                base::CompareCase::SENSITIVE)) {
      // Let the page be served directly
      return nullptr;
    } else {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader(
          "Location",
          HttpsURLWithPath("/load_image/private_url_image.png").spec());
    }
    return std::move(response);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  bool enable_subresource_server_redirect_ = false;

  GURL compression_url_;
  GURL http_url_;
  GURL https_url_;
  GURL request_url_;

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer compression_server_;

  base::HistogramTester histogram_tester_;

  // Whether the embedded test servers should return failure.
  bool https_server_image_fail_ = false;
  bool compression_server_fail_ = false;

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceRedirectBrowserTest);
};

class RedirectDisabledSubresourceRedirectBrowserTest
    : public SubresourceRedirectBrowserTest {
 public:
  RedirectDisabledSubresourceRedirectBrowserTest()
      : SubresourceRedirectBrowserTest(false) {}
};

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

//  NOTE: It is indirectly verified that correct requests are being sent to
//  the mock compression server by the counts in the histogram bucket for
//  HTTP_TEMPORARY_REDIRECTs.

//  This test loads image.html, which triggers a subresource request
//  for image.png.  This triggers an internal redirect to the mocked
//  compression server, which responds with HTTP_OK.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestHTMLLoadRedirectSuccess)) {
  EnableDataSaver(true);
  CreateUkmRecorder();

  GURL url = HttpsURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  EXPECT_EQ(request_url().port(), compression_url().port());
  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

//  This test loads private_url_image.html, which triggers a subresource
//  request for private_url_image.png.  This triggers an internal redirect
//  to the mock compression server, which bypasses the request. The
//  mock compression server creates a redirect to the original resource.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestHTMLLoadRedirectBypass)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/private_url_image.html");
  SetUpPublicImageURLPaths(url, {"/load_image/private_url_image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
  // The image will be marked as compressible even though the private image
  // redirect was bypassed.
  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       NoTriggerWhenDataSaverOff) {
  EnableDataSaver(false);
  CreateUkmRecorder();
  ui_test_utils::NavigateToURL(
      browser(), HttpsURLWithPath("/load_image/image_delayed_load.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  // No coverage metrics recorded.
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest, NoTriggerInIncognito) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  auto* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(
      incognito_browser,
      HttpsURLWithPath("/load_image/image_delayed_load.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool(
      "checkImage()",
      incognito_browser->tab_strip_model()->GetActiveWebContents()));

  EXPECT_EQ(
      GURL(RunScriptExtractString(
               "imageSrc()",
               incognito_browser->tab_strip_model()->GetActiveWebContents()))
          .port(),
      https_url().port());

  // No coverage metrics recorded.
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

//  This test loads image.html, from a non secure site. This triggers a
//  subresource request, but no internal redirect should be created for
//  non-secure sites.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       NoTriggerOnNonSecureSite) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            http_url().port());

  // No coverage metrics recorded.
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

//  This test loads page_with_favicon.html, which creates a subresource
//  request for icon.png.  There should be no internal redirect as favicons
//  are not considered images by chrome.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest, NoTriggerOnNonImage) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/favicon/page_with_favicon.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  // No coverage metrics recorded.
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

}  // namespace

// This test loads a resource that will return a 404 from the server, this
// should trigger the fallback logic back to the original resource. In total
// This results in 2 redirects (to the compression server, and back to the
// original resource), 1 404 not-found from the compression server, and 1
// 200 ok from the original resource.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(FallbackOnServerNotFound)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/fail_image.html");
  SetUpPublicImageURLPaths(url, {"/load_image/fail_image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 3);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_NOT_FOUND, 1);

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  // Ineligible Other bucket ukm recorded.
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(1);
}

//  This test verifies that the client will utilize the fallback logic if the
//  compression server fails and returns nothing.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(FallbackOnServerFailure)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  SetCompressionServerToFail();

  base::RunLoop().RunUntilIdle();
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  RetryForHistogramUntilCountReached(
      histogram_tester(),
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", false, 1);

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  // Ineligible Other bucket ukm recorded.
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(1);
}

//  This test verifies that the client will utilize the fallback logic if the
//  compression server and the main https server fails and returns nothing.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(FallbackOnBothServerFailure)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  SetHttpsServerImageToFail();
  SetCompressionServerToFail();

  base::RunLoop().RunUntilIdle();
  ui_test_utils::NavigateToURL(browser(), url);

  // The image should failed to load, but some redirect metrics are recorded.
  EXPECT_FALSE(RunScriptExtractBool("checkImage()"));
  RetryForHistogramUntilCountReached(
      histogram_tester(),
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 1);
  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", false, 1);

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  // No coverage ukm is recorded, since the image load failed.
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// This test verifies that accessing the compression server directly will not do
// any additional redirection.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestDirectCompressionServerPage)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});

  base::RunLoop().RunUntilIdle();
  ui_test_utils::NavigateToURL(
      browser(),
      GetSubresourceURLForURL("/load_image/image_delayed_load.html"));

  // The image should failed to load, but some redirect
  // metrics are recorded.
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt."
      "ServerResponded",
      0);

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            compression_url().port());

  // The coverage ukm still gets recorded.
  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestTwoPublicImagesAreRedirected)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/two_images.html");
  SetUpPublicImageURLPaths(
      url, {"/load_image/image.png", "/load_image/image.png?foo"});
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      4);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  EXPECT_TRUE(RunScriptExtractBool("checkBothImagesLoaded()"));
  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  VerifyCompressibleImageUkm(2);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// This test verifies that only the images in the public image URL list are
// is_subresource_redirect_feature_enabled. In this test both images should load
// but only one image should be redirected.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestOnlyPublicImageIsRedirected)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/two_images.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkBothImagesLoaded()"));
  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(1);
  VerifyIneligibleOtherImageUkm(0);
}

// This test verifies that the fragments in the image URL are removed before
// checking against the public image URL list.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestImageURLFragmentAreRemoved)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_with_fragment.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

//  This test loads image_js.html, which triggers a javascript request
//  for image.png for which subresource redirect will not be attempted.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       NoTriggerOnJavaScriptImageRequest) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_js.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  VerifyIneligibleOtherImageUkm(1);
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
}

// This test verifies that no image redirect happens when empty hints is sent.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestNoRedirectWithEmptyHints)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {});
  ui_test_utils::NavigateToURL(browser(), url);

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.DidCompress.CompressionPercent", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  VerifyIneligibleMissingInImageHintsUkm(1);
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// This test verifies that no image redirect happens when hints are not yet
// received.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestNoRedirectWithoutHints) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  ui_test_utils::NavigateToURL(
      browser(), HttpsURLWithPath("/load_image/image_delayed_load.html"));

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.DidCompress.CompressionPercent", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
  WaitForImageCompressionUkmMetrics(1);
  VerifyIneligibleImageHintsUnavailableUkm(1);
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// This test verifies that two images in a page are not redirected, when hints
// are missing.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestNoRedirectWithoutHintsTwoImages) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/two_images.html"));

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.DidCompress.CompressionPercent", 0);
  EXPECT_TRUE(RunScriptExtractBool("checkBothImagesLoaded()"));
  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
  WaitForImageCompressionUkmMetrics(2);
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(2);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// This test initiates same-origin navigation and verifies the hints from the
// previous navigation are not used.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestSameOriginNavigation)) {
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  EXPECT_EQ(request_url().port(), compression_url().port());
  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);

  // Initiate a same-origin navigation without hints, and let the timeout ukm be
  // recorded.
  CreateUkmRecorder();
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/two_images.html"));

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 2);
  EXPECT_TRUE(RunScriptExtractBool("checkBothImagesLoaded()"));
  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
  WaitForImageCompressionUkmMetrics(2);
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(2);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// This test verifies that the image redirect to lite page is disabled via
// finch, and only the coverage metrics are recorded.
IN_PROC_BROWSER_TEST_F(RedirectDisabledSubresourceRedirectBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ImagesNotRedirected)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/image_delayed_load.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURL(browser(), url);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.DidCompress.CompressionPercent", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// This class sets up a hints server where image hints are fetched from to test
// the page level hint fetches.
class SubresourceRedirectWithHintsServerBrowserTest
    : public SubresourceRedirectBrowserTest {
 public:
  // How the hints server should respond to the get hints request.
  enum HintFetchMode {
    // Delay the hints fetch until images are loaded. Delay 3 seconds before
    // sending response. This delay is chosen such that the server sends the
    // hints after the image has completed loading but before the hints receive
    // timeout(5 seconds).
    HINT_FETCH_AFTER_IMAGES_LOADED,

    // Do not send response.
    HINT_FETCH_HUNG,
  };

  SubresourceRedirectWithHintsServerBrowserTest()
      : SubresourceRedirectBrowserTest(true),
        hints_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    hints_server_.ServeFilesFromSourceDirectory("chrome/test/data/previews");
    hints_server_.RegisterRequestHandler(base::BindRepeating(
        &SubresourceRedirectWithHintsServerBrowserTest::HandleGetHintsRequest,
        base::Unretained(this)));
    ASSERT_TRUE(hints_server_.Start());
    SubresourceRedirectBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");
    command_line->AppendSwitch("purge_hint_cache_store");
    command_line->AppendSwitch(optimization_guide::switches::
                                   kDisableCheckingUserPermissionsForTesting);
    command_line->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
        hints_server_.base_url().spec());
    command_line->AppendSwitchASCII(
        optimization_guide::switches::kFetchHintsOverride, "secure.com");
    command_line->AppendSwitch(
        optimization_guide::switches::kFetchHintsOverrideTimer);
    SubresourceRedirectBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_2G);
    SubresourceRedirectBrowserTest::SetUpOnMainThread();
  }

  // Sets up public image URL hint data that is returned by the hints server.
  void SetUpPublicImageURLPaths(
      const std::string& url,
      const std::vector<std::string>& public_image_paths,
      HintFetchMode hint_fetch_mode) {
    hint_fetch_mode_ = hint_fetch_mode;
    optimization_guide::proto::GetHintsResponse get_hints_response;
    optimization_guide::proto::Hint* hint = get_hints_response.add_hints();
    hint->set_key_representation(optimization_guide::proto::FULL_URL);
    hint->set_key(HttpsURLWithPath(url).spec());
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern(HttpsURLWithPath(url).spec());
    auto* optimization = page_hint->add_whitelisted_optimizations();
    optimization->set_optimization_type(
        optimization_guide::proto::OptimizationType::COMPRESS_PUBLIC_IMAGES);
    auto* public_image_metadata = optimization->mutable_public_image_metadata();

    for (const auto& url : public_image_paths)
      public_image_metadata->add_url(HttpsURLWithPath(url).spec());

    base::AutoLock lock(lock_);
    get_hints_response.SerializeToString(&get_hints_response_);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGetHintsRequest(
      const net::test_server::HttpRequest& request) {
    switch (hint_fetch_mode_) {
      case HintFetchMode::HINT_FETCH_AFTER_IMAGES_LOADED: {
        auto response = std::make_unique<net::test_server::DelayedHttpResponse>(
            base::TimeDelta::FromSeconds(3));
        response->set_content(get_hints_response_);
        response->set_code(net::HTTP_OK);
        return std::move(response);
      }
      case HintFetchMode::HINT_FETCH_HUNG:
        return std::make_unique<net::test_server::HungResponse>();
    }
    return nullptr;
  }

  net::EmbeddedTestServer hints_server_;
  std::string get_hints_response_;
  base::Lock lock_;
  HintFetchMode hint_fetch_mode_ =
      HintFetchMode::HINT_FETCH_AFTER_IMAGES_LOADED;
};

// This test verifies that two images in a page are not redirected, when hints
// are received delayed.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectWithHintsServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestNoRedirectWithDelayedHintsTwoImages)) {
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  EnableDataSaver(true);
  CreateUkmRecorder();
  SetUpPublicImageURLPaths("/load_image/two_images.html",
                           {"/load_image/image.png"},
                           HintFetchMode::HINT_FETCH_AFTER_IMAGES_LOADED);
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/two_images.html"));

  // Let the images load.
  EXPECT_TRUE(RunScriptExtractBool("checkBothImagesLoaded()"));

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.DidCompress.CompressionPercent", 0);
  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());

  // One image will be recorded as compressible, but image hints not received in
  // time. Another image is recorded as not compressible, and image hints not
  // received in time.
  WaitForImageCompressionUkmMetrics(2);
  VerifyIneligibleImageHintsUnavailableUkmButCompressible(1);
  VerifyIneligibleImageHintsUnavailableAndMissingInHintsUkm(1);
  VerifyCompressibleImageUkm(0);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// Tests CSS background images are redirected.
// Disabled due to flakes. See https://crbug.com/1063736.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestCSSBackgroundImageRedirect)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/css_background_image.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);
  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_EQ(request_url().port(), compression_url().port());
  WaitForImageCompressionUkmMetrics(1);
  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}

// Tests CSS background image coverage metrics is recorded but not redirected,
// when redirect is disabled.
// Disabling for all as it was already Disabled on Mac, Win and ChromeOS and it
// now seems to be flaky on Linux
// Disabled due to flakes. See https://crbug.com/1063736.
IN_PROC_BROWSER_TEST_F(
    RedirectDisabledSubresourceRedirectBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestCSSBackgroundImageRedirect)) {
  EnableDataSaver(true);
  CreateUkmRecorder();
  GURL url = HttpsURLWithPath("/load_image/css_background_image.html");
  SetUpPublicImageURLPaths(url, {"/load_image/image.png"});
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.DidCompress.CompressionPercent", 0);

  WaitForImageCompressionUkmMetrics(1);
  VerifyCompressibleImageUkm(1);
  VerifyIneligibleImageHintsUnavailableUkm(0);
  VerifyIneligibleMissingInImageHintsUkm(0);
  VerifyIneligibleOtherImageUkm(0);
}
