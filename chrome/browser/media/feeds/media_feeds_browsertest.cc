// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/feeds/media_feeds_contents_observer.h"
#include "chrome/browser/media/history/media_history_feeds_table.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace media_feeds {

namespace {

const char kMediaFeedsTestURL[] = "/media-feed";

const char kMediaFeedsTestHTML[] =
    "  <!DOCTYPE html>"
    "  <head>%s</head>";

struct TestData {
  std::string head_html;
  bool discovered;
  bool https = true;
};

}  // namespace

class MediaFeedsBrowserTest : public InProcessBrowserTest,
                              public ::testing::WithParamInterface<TestData> {
 public:
  MediaFeedsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~MediaFeedsBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(media::kMediaFeeds);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // The HTTPS server serves the test page using HTTPS.
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &MediaFeedsBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());

    // The embedded test server will serve the test page using HTTP.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &MediaFeedsBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  std::set<GURL> GetDiscoveredFeedURLs() {
    base::RunLoop run_loop;
    std::set<GURL> out;

    GetMediaHistoryService()->GetURLsInTableForTest(
        media_history::MediaHistoryFeedsTable::kTableName,
        base::BindLambdaForTesting([&](std::set<GURL> urls) {
          out = urls;
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  media_history::MediaHistoryKeyedService* GetMediaHistoryService() {
    return media_history::MediaHistoryKeyedServiceFactory::GetForProfile(
        browser()->profile());
  }

  net::EmbeddedTestServer* GetServer() {
    return GetParam().https ? &https_server_ : embedded_test_server();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kMediaFeedsTestURL)
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(
        base::StringPrintf(kMediaFeedsTestHTML, GetParam().head_html.c_str()));
    return response;
  }

  net::EmbeddedTestServer https_server_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaFeedsBrowserTest,
    ::testing::Values(
        TestData{"<link rel=feed type=\"application/ld+json\" href=\"/test\"/>",
                 true},
        TestData{"<link rel=feed type=\"application/ld+json\" href=\"/test\"/>",
                 false, false},
        TestData{"", false},
        TestData{"<link rel=feed type=\"application/ld+json\" "
                 "href=\"/test\"/><link rel=feed "
                 "type=\"application/ld+json\" href=\"/test2\"/>",
                 true},
        TestData{"<link rel=feed type=\"application/ld+json\" "
                 "href=\"https://www.example.com/test\"/>",
                 false},
        TestData{"<link rel=feed type=\"application/ld+json\" href=\"\"/>",
                 false},
        TestData{"<link rel=feed href=\"/test\"/>", false},
        TestData{
            "<link rel=other type=\"application/ld+json\" href=\"/test\"/>",
            false}));

IN_PROC_BROWSER_TEST_P(MediaFeedsBrowserTest, Discover) {
  EXPECT_TRUE(GetDiscoveredFeedURLs().empty());

  MediaFeedsContentsObserver* contents_observer =
      static_cast<MediaFeedsContentsObserver*>(
          MediaFeedsContentsObserver::FromWebContents(GetWebContents()));

  GURL test_url(GetServer()->GetURL(kMediaFeedsTestURL));

  // The contents observer will call this closure when it has checked for a
  // media feed.
  base::RunLoop run_loop;

  contents_observer->SetClosureForTest(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  ui_test_utils::NavigateToURL(browser(), test_url);

  run_loop.Run();

  // Wait until the session has finished saving.
  content::RunAllTasksUntilIdle();

  // Check we discovered the feed.
  std::set<GURL> expected_urls;

  if (GetParam().discovered)
    expected_urls.insert(GetServer()->GetURL("/test"));

  EXPECT_EQ(expected_urls, GetDiscoveredFeedURLs());
}

}  // namespace media_feeds
