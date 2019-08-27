// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_url_loader_interceptor.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/961073): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_InterceptRequestPreviewsState_PreviewsOff \
  DISABLED_InterceptRequestPreviewsState_PreviewsOff
#define MAYBE_InterceptRequestPreviewsState_ProbeSuccess \
  DISABLED_InterceptRequestPreviewsState_ProbeSuccess
#define MAYBE_InterceptRequestPreviewsState_ProbeFail \
  DISABLED_InterceptRequestPreviewsState_ProbeFail
#else
#define MAYBE_InterceptRequestPreviewsState_PreviewsOff \
  InterceptRequestPreviewsState_PreviewsOff
#define MAYBE_InterceptRequestPreviewsState_ProbeSuccess \
  InterceptRequestPreviewsState_ProbeSuccess
#define MAYBE_InterceptRequestPreviewsState_ProbeFail \
  InterceptRequestPreviewsState_ProbeFail
#endif

namespace previews {

namespace {

const GURL kTestUrl("https://google.com/path");

class PreviewsLitePageURLLoaderInterceptorTest : public testing::Test {
 public:
  PreviewsLitePageURLLoaderInterceptorTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~PreviewsLitePageURLLoaderInterceptorTest() override {}

  void TearDown() override {}

  void SetUp() override {
    interceptor_ = std::make_unique<PreviewsLitePageURLLoaderInterceptor>(
        shared_factory_, 1, 2);

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kLitePageServerPreviews, {{"should_probe_origin", "true"}});
  }

  void SetFakeResponse(const GURL& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       int net_error) {
    test_url_loader_factory_.AddResponse(
        url, network::CreateResourceResponseHead(code), data,
        network::URLLoaderCompletionStatus(net_error));
  }

  void SetProbeResponse(const GURL& url,
                        net::HttpStatusCode code,
                        int net_error) {
    test_url_loader_factory_.AddResponse(
        url, network::CreateResourceResponseHead(code), "data",
        network::URLLoaderCompletionStatus(net_error));
  }

  void HandlerCallback(
      content::URLLoaderRequestInterceptor::RequestHandler callback) {
    callback_was_empty_ = callback.is_null();
  }

  base::Optional<bool> callback_was_empty() { return callback_was_empty_; }

  PreviewsLitePageURLLoaderInterceptor& interceptor() { return *interceptor_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  base::Optional<bool> callback_was_empty_;

  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<PreviewsLitePageURLLoaderInterceptor> interceptor_;
};

// Check that we don't trigger when previews are not allowed.
TEST_F(PreviewsLitePageURLLoaderInterceptorTest,
       InterceptRequestPreviewsState_PreviewsOff) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  SetFakeResponse(request.url, "Fake Body", net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  request.previews_state = content::PREVIEWS_OFF;
  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", false, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

// Check that we trigger when previews are allowed and the probe is successful.
TEST_F(PreviewsLitePageURLLoaderInterceptorTest,
       MAYBE_InterceptRequestPreviewsState_ProbeSuccess) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_FALSE(callback_was_empty().value());
  LOG(ERROR) << "test end";
}

TEST_F(PreviewsLitePageURLLoaderInterceptorTest,
       InterceptRequestPreviewsState_ProbeFail) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::FAILED);

  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageURLLoaderInterceptorTest, InterceptRequestRedirect) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_TEMPORARY_REDIRECT,
      net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageURLLoaderInterceptorTest,
       InterceptRequestServerOverloaded) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_SERVICE_UNAVAILABLE,
      net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageURLLoaderInterceptorTest,
       InterceptRequestServerNotHandling) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_FORBIDDEN, net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageURLLoaderInterceptorTest, NetStackError) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_OK, net::URLRequestStatus::FAILED);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

}  // namespace

}  // namespace previews
