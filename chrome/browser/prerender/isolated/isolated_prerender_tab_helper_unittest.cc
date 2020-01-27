// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/prefetched_response_container.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char kHTMLMimeType[] = "text/html";

const char kHTMLBody[] = R"(
      <!DOCTYPE HTML>
      <html>
        <head></head>
        <body></body>
      </html>)";
}  // namespace

class IsolatedPrerenderTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  IsolatedPrerenderTabHelperTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~IsolatedPrerenderTabHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    IsolatedPrerenderTabHelper::CreateForWebContents(web_contents());

    IsolatedPrerenderTabHelper::FromWebContents(web_contents())
        ->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);
  }

  void MakeNavigationPrediction(const content::WebContents* web_contents,
                                const GURL& doc_url,
                                const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(profile())
        ->OnPredictionUpdated(web_contents, doc_url, predicted_urls);
    task_environment()->RunUntilIdle();
  }

  int RequestCount() { return test_url_loader_factory_.NumPending(); }

  void VerifyCommonRequestState(const GURL& url) {
    ASSERT_EQ(RequestCount(), 1);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    EXPECT_EQ(request->request.url, url);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_EQ(request->request.load_flags,
              net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH);
    EXPECT_EQ(request->request.credentials_mode,
              network::mojom::CredentialsMode::kOmit);
  }

  std::string RequestHeader(const std::string& key) {
    if (test_url_loader_factory_.NumPending() != 1)
      return std::string();

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    std::string value;
    if (request->request.headers.GetHeader(key, &value))
      return value;

    return std::string();
  }

  void MakeResponseAndWait(net::HttpStatusCode http_status,
                           net::Error net_error,
                           const std::string& mime_type,
                           std::vector<std::string> headers,
                           const std::string& body) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);

    auto head = network::CreateURLResponseHead(http_status);
    head->mime_type = mime_type;
    for (const std::string& header : headers) {
      head->headers->AddHeader(header);
    }
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         body, status);
    task_environment()->RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void ClearResponses() { test_url_loader_factory_.ClearResponses(); }

  bool SetCookie(content::BrowserContext* browser_context,
                 const GURL& url,
                 const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    content::BrowserContext::GetDefaultStoragePartition(browser_context)
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
    std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), base::nullopt /* server_time */));
    EXPECT_TRUE(cc.get());

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
    cookie_manager->SetCanonicalCookie(
        *cc.get(), url.scheme(), options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CanonicalCookie::CookieInclusionStatus set_cookie_status) {
              *result = set_cookie_status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(IsolatedPrerenderTabHelperTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, GoogleSRPOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.not-google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, SRPOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/photos?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, HTTPSPredictionsOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.not-google.com/search?q=cats");
  GURL prediction_url("http://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, DontFetchGoogleLinks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("http://www.google.com/user");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, DontFetchIPAddresses) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://123.234.123.234/meow");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, WrongWebContents) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(nullptr, doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, HasPurposePrefetchHeader) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  EXPECT_EQ(RequestHeader("Purpose"), "prefetch");
}

TEST_F(IsolatedPrerenderTabHelperTest, NoCookies) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  ASSERT_TRUE(SetCookie(profile(), prediction_url, "testing"));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  EXPECT_EQ(RequestHeader("Cookie"), std::string());
}

TEST_F(IsolatedPrerenderTabHelperTest, NoRedirects) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_TEMPORARY_REDIRECT, net::OK, kHTMLMimeType,
                      {"Location: https://foo.com"}, "unused body");
  // Redirect should not be followed.
  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(IsolatedPrerenderTabHelper::FromWebContents(web_contents())
                ->prefetched_responses_size_for_testing(),
            0U);
}

TEST_F(IsolatedPrerenderTabHelperTest, 2XXOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);
  EXPECT_EQ(IsolatedPrerenderTabHelper::FromWebContents(web_contents())
                ->prefetched_responses_size_for_testing(),
            0U);
}

TEST_F(IsolatedPrerenderTabHelperTest, NetErrorOKOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);
  EXPECT_EQ(IsolatedPrerenderTabHelper::FromWebContents(web_contents())
                ->prefetched_responses_size_for_testing(),
            0U);
}

TEST_F(IsolatedPrerenderTabHelperTest, NonHTML) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/javascript",
                      /*headers=*/{},
                      /*body=*/"console.log('Hello world');");
  EXPECT_EQ(IsolatedPrerenderTabHelper::FromWebContents(web_contents())
                ->prefetched_responses_size_for_testing(),
            0U);
}

TEST_F(IsolatedPrerenderTabHelperTest, SuccessCase) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchSRPNavigationPredictions_HTMLOnly);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      {"X-Testing: Hello World"}, kHTMLBody);

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(web_contents());
  EXPECT_EQ(tab_helper->prefetched_responses_size_for_testing(), 1U);

  std::unique_ptr<PrefetchedResponseContainer> resp =
      tab_helper->TakePrefetchResponse(prediction_url);
  ASSERT_TRUE(resp);
  EXPECT_EQ(*resp->TakeBody(), kHTMLBody);

  network::mojom::URLResponseHeadPtr head = resp->TakeHead();
  EXPECT_TRUE(head->headers->HasHeaderValue("X-Testing", "Hello World"));
}
