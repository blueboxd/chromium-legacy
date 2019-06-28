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
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/961073): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_InterceptRequestPreviewsState \
  DISABLED_InterceptRequestPreviewsState
#else
#define MAYBE_InterceptRequestPreviewsState InterceptRequestPreviewsState
#endif

namespace previews {

namespace {

class PreviewsLitePageURLLoaderInterceptorTest : public testing::Test {
 public:
  PreviewsLitePageURLLoaderInterceptorTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~PreviewsLitePageURLLoaderInterceptorTest() override {}

  void TearDown() override {}

  void SetUp() override {
    interceptor_ = std::make_unique<PreviewsLitePageURLLoaderInterceptor>(
        shared_factory_, 1, 2);
  }

  void SetFakeResponse(const GURL& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       int net_error) {
    test_url_loader_factory_.AddResponse(
        url, network::CreateResourceResponseHead(code), data,
        network::URLLoaderCompletionStatus(net_error));
  }

  void HandlerCallback(
      content::URLLoaderRequestInterceptor::RequestHandler callback) {
    callback_was_empty_ = callback.is_null();
  }

  base::Optional<bool> callback_was_empty() { return callback_was_empty_; }

  void ResetTest() {
    interceptor_ = std::make_unique<PreviewsLitePageURLLoaderInterceptor>(
        shared_factory_, 1, 2);
    callback_was_empty_ = base::nullopt;
  }

  PreviewsLitePageURLLoaderInterceptor& interceptor() { return *interceptor_; }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

 private:
  base::Optional<bool> callback_was_empty_;

  std::unique_ptr<PreviewsLitePageURLLoaderInterceptor> interceptor_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
};

TEST_F(PreviewsLitePageURLLoaderInterceptorTest,
       MAYBE_InterceptRequestPreviewsState) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = GURL("https://google.com");
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Check that we don't trigger when previews are not allowed.
  request.previews_state = content::PREVIEWS_OFF;
  interceptor().MaybeCreateLoader(
      request, nullptr, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", false, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());

  ResetTest();
  SetFakeResponse(request.url, "Fake Body", net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  // Check that we trigger when previews are allowed.
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  interceptor().MaybeCreateLoader(
      request, nullptr, nullptr,
      base::BindOnce(&PreviewsLitePageURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));

  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);
  histogram_tester.ExpectTotalCount(
      "Previews.ServerLitePage.URLLoader.Attempted", 2);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_FALSE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageURLLoaderInterceptorTest, InterceptRequestRedirect) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = GURL("https://google.com");
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_TEMPORARY_REDIRECT,
      net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr, nullptr,
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
  request.url = GURL("https://google.com");
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_SERVICE_UNAVAILABLE,
      net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr, nullptr,
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
  request.url = GURL("https://google.com");
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_FORBIDDEN, net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr, nullptr,
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
  request.url = GURL("https://google.com");
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(request.url),
      "Fake Body", net::HTTP_OK, net::URLRequestStatus::FAILED);

  interceptor().MaybeCreateLoader(
      request, nullptr, nullptr,
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
