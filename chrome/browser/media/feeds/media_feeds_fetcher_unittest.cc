// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_fetcher.h"

#include <memory>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/schema_org/common/metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "content/public/browser/storage_partition.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_feeds {

using testing::_;

const char kTestUrl[] = "https://www.google.com";

class MediaFeedsFetcherTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFeedsFetcherTest() = default;
  ~MediaFeedsFetcherTest() override = default;
  MediaFeedsFetcherTest(const MediaFeedsFetcherTest& t) = delete;
  MediaFeedsFetcherTest& operator=(const MediaFeedsFetcherTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    fetcher_ = std::make_unique<MediaFeedsFetcher>(
        base::MakeRefCounted<::network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory()));
  }

  MediaFeedsFetcher* fetcher() { return fetcher_.get(); }
  ::network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  bool RespondToFetch(
      const std::string& response_body,
      net::HttpStatusCode response_code = net::HttpStatusCode::HTTP_OK,
      int net_error = net::OK,
      bool was_fetched_via_cache = false) {
    auto response_head = ::network::CreateURLResponseHead(response_code);
    response_head->was_fetched_via_cache = was_fetched_via_cache;

    bool rv = url_loader_factory()->SimulateResponseForPendingRequest(
        GURL(kTestUrl), ::network::URLLoaderCompletionStatus(net_error),
        std::move(response_head), response_body);
    task_environment()->RunUntilIdle();
    return rv;
  }

  void WaitForRequest() {
    task_environment()->RunUntilIdle();

    ASSERT_TRUE(GetCurrentRequest().url.is_valid());
    EXPECT_TRUE(GetCurrentRequest().site_for_cookies.IsEquivalent(
        net::SiteForCookies::FromUrl(GURL(kTestUrl))));
    EXPECT_EQ(GetCurrentlyQueriedHeaderValue(net::HttpRequestHeaders::kAccept),
              "application/ld+json");
    EXPECT_EQ(GetCurrentRequest().redirect_mode,
              ::network::mojom::RedirectMode::kError);
    EXPECT_EQ(net::HttpRequestHeaders::kGetMethod, GetCurrentRequest().method);
  }

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

    cookie_manager->SetCanonicalCookie(
        *cc.get(), url, net::CookieOptions::MakeAllInclusive(),
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
  std::string GetCurrentlyQueriedHeaderValue(const base::StringPiece& key) {
    std::string out;
    GetCurrentRequest().headers.GetHeader(key, &out);
    return out;
  }

  const ::network::ResourceRequest& GetCurrentRequest() {
    return url_loader_factory()->pending_requests()->front().request;
  }

  ::network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<MediaFeedsFetcher> fetcher_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

TEST_F(MediaFeedsFetcherTest, SucceedsOnBasicFetch) {
  GURL site_with_cookies(kTestUrl);
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));

  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  schema_org::improved::mojom::EntityPtr expected =
      schema_org::improved::mojom::Entity::New();
  expected->type = schema_org::entity::kCompleteDataFeed;
  schema_org::improved::mojom::PropertyPtr property =
      schema_org::improved::mojom::Property::New();
  property->name = "name";
  property->values = schema_org::improved::mojom::Values::New();
  property->values->string_values.push_back("Media Site");
  expected->properties.push_back(std::move(property));

  schema_org::improved::mojom::EntityPtr out;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kOk);
            EXPECT_FALSE(was_fetched_via_cache);
            out = response.Clone();
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(
      "{\"@type\":\"CompleteDataFeed\",\"name\":\"Media Site\"}"));

  EXPECT_EQ(out, expected);
}

TEST_F(MediaFeedsFetcherTest, SucceedsOnBasicFetch_ForceCache) {
  GURL site_with_cookies(kTestUrl);
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));

  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  schema_org::improved::mojom::EntityPtr expected =
      schema_org::improved::mojom::Entity::New();
  expected->type = schema_org::entity::kCompleteDataFeed;
  schema_org::improved::mojom::PropertyPtr property =
      schema_org::improved::mojom::Property::New();
  property->name = "name";
  property->values = schema_org::improved::mojom::Values::New();
  property->values->string_values.push_back("Media Site");
  expected->properties.push_back(std::move(property));

  schema_org::improved::mojom::EntityPtr out;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), true,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kOk);
            EXPECT_FALSE(was_fetched_via_cache);
            out = response.Clone();
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(
      "{\"@type\":\"CompleteDataFeed\",\"name\":\"Media Site\"}"));

  EXPECT_EQ(out, expected);
}

TEST_F(MediaFeedsFetcherTest, SucceedsFetchFromCache) {
  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kOk);
            EXPECT_TRUE(was_fetched_via_cache);
            EXPECT_TRUE(response);
          }));

  WaitForRequest();
  ASSERT_TRUE(
      RespondToFetch("{\"@type\":\"CompleteDataFeed\",\"name\":\"Media Site\"}",
                     net::HttpStatusCode::HTTP_OK, net::OK, true));
}

TEST_F(MediaFeedsFetcherTest, ReturnsFailedResponseCode) {
  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kRequestFailed);
            EXPECT_FALSE(was_fetched_via_cache);
            EXPECT_FALSE(response);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_BAD_REQUEST));
}

TEST_F(MediaFeedsFetcherTest, ReturnsGone) {
  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kGone);
            EXPECT_FALSE(was_fetched_via_cache);
            EXPECT_FALSE(response);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_GONE));
}

TEST_F(MediaFeedsFetcherTest, ReturnsNetError) {
  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kRequestFailed);
            EXPECT_FALSE(was_fetched_via_cache);
            EXPECT_FALSE(response);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_OK, net::ERR_UNEXPECTED));
}

TEST_F(MediaFeedsFetcherTest, ReturnsErrFileNotFoundForEmptyFeedData) {
  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kNotFound);
            EXPECT_FALSE(was_fetched_via_cache);
            EXPECT_FALSE(response);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(""));
}

TEST_F(MediaFeedsFetcherTest, ReturnsErrFailedForBadEntityData) {
  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](const schema_org::improved::mojom::EntityPtr& response,
              MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
            EXPECT_EQ(status, MediaFeedsFetcher::Status::kInvalidFeedData);
            EXPECT_FALSE(was_fetched_via_cache);
            EXPECT_FALSE(response);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(
      "{\"@type\":\"CompleteDataFeed\"\"name\":\"Bad json missing a comma\"}"));
}

}  // namespace media_feeds
