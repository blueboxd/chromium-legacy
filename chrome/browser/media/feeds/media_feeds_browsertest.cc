// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/feeds/media_feeds_contents_observer.h"
#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/history/media_history_feeds_table.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_feeds {

namespace {

const char kMediaFeedsTestURL[] = "/test";

const char kMediaFeedsAltTestURL[] = "/alt";

constexpr base::FilePath::CharType kMediaFeedsTestFileName[] =
    FILE_PATH_LITERAL("chrome/test/data/media/feeds/media-feed.json");

const char kMediaFeedsTestHTML[] =
    "  <!DOCTYPE html>"
    "  <head>%s</head>";

const char kMediaFeedsTestHeadHTML[] =
    "<link rel=feed type=\"application/ld+json\" "
    "href=\"/media-feed.json\"/>";

struct TestData {
  std::string head_html;
  bool discovered;
  bool https = true;
};

}  // namespace

class MediaFeedsBrowserTest : public InProcessBrowserTest {
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

  std::vector<media_feeds::mojom::MediaFeedPtr> GetDiscoveredFeeds() {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedPtr> out;

    GetMediaHistoryService()->GetMediaFeeds(
        media_history::MediaHistoryKeyedService::GetMediaFeedsRequest(),
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedPtr> feeds) {
              out = std::move(feeds);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  void WaitForDB() {
    base::RunLoop run_loop;
    GetMediaHistoryService()->PostTaskToDBForTest(run_loop.QuitClosure());
    run_loop.Run();
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

  void DiscoverFeed() {
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
    WaitForDB();
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      int64_t feed_id) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedItemPtr> out;

    GetMediaHistoryService()->GetItemsForMediaFeedForDebug(
        feed_id,
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedItemPtr> items) {
              out = std::move(items);
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

  media_feeds::MediaFeedsService* GetMediaFeedsService() {
    return media_feeds::MediaFeedsService::Get(browser()->profile());
  }

  virtual net::EmbeddedTestServer* GetServer() { return &https_server_; }

 private:
  virtual std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == kMediaFeedsTestURL) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(
          base::StringPrintf(kMediaFeedsTestHTML, kMediaFeedsTestHeadHTML));
      return response;
    } else if (base::EndsWith(request.relative_url, "json",
                              base::CompareCase::SENSITIVE)) {
      if (full_test_data_.empty())
        LoadFullTestData();
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(full_test_data_);
      return response;
    } else if (request.relative_url == kMediaFeedsAltTestURL) {
      return std::make_unique<net::test_server::BasicHttpResponse>();
    }
    return nullptr;
  }

  void LoadFullTestData() {
    base::FilePath file;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &file));
    file = file.Append(kMediaFeedsTestFileName);

    base::ReadFileToString(file, &full_test_data_);
    ASSERT_TRUE(full_test_data_.size());
  }

  net::EmbeddedTestServer https_server_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::string full_test_data_;
};

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest, DiscoverAndFetch) {
  DiscoverFeed();

  // Check we discovered the feed.
  std::set<GURL> expected_urls = {GetServer()->GetURL("/media-feed.json")};
  EXPECT_EQ(expected_urls, GetDiscoveredFeedURLs());

  std::vector<media_feeds::mojom::MediaFeedPtr> discovered_feeds =
      GetDiscoveredFeeds();
  EXPECT_EQ(1u, discovered_feeds.size());

  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(discovered_feeds[0]->id,
                                         run_loop.QuitClosure());
  run_loop.Run();
  WaitForDB();

  auto items = GetItemsForMediaFeedSync(discovered_feeds[0]->id);

  EXPECT_EQ(7u, items.size());
  std::vector<std::string> names;
  std::transform(items.begin(), items.end(), std::back_inserter(names),
                 [](auto& item) { return base::UTF16ToASCII(item->name); });
  EXPECT_THAT(names, testing::UnorderedElementsAre(
                         "Anatomy of a Web Media Experience",
                         "Building Modern Web Media Experiences: "
                         "Picture-in-Picture and AV1",
                         "Chrome Releases", "Chrome University", "JAM stack",
                         "Ask Chrome", "Big Buck Bunny"));
}

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest, ResetMediaFeed_OnNavigation) {
  DiscoverFeed();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

    // Fetch the feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(feeds[0]->id,
                                           run_loop.QuitClosure());
    run_loop.Run();
    WaitForDB();
  }

  // Navigate on the same origin and make sure we do not reset.
  ui_test_utils::NavigateToURL(browser(),
                               GetServer()->GetURL(kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  // Navigate to a different origin and make sure we reset.
  ui_test_utils::NavigateToURL(
      browser(), GetServer()->GetURL("www.example.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
  }
}

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest,
                       ResetMediaFeed_OnNavigation_NeverFetched) {
  DiscoverFeed();

  ui_test_utils::NavigateToURL(
      browser(), GetServer()->GetURL("www.example.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest,
                       ResetMediaFeed_OnNavigation_WrongOrigin) {
  DiscoverFeed();

  ui_test_utils::NavigateToURL(
      browser(), GetServer()->GetURL("www.example.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

    // Fetch the feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(feeds[0]->id,
                                           run_loop.QuitClosure());
    run_loop.Run();
    WaitForDB();
  }

  // The navigation is not on an origin associated with the feed so we should
  // never reset it.
  ui_test_utils::NavigateToURL(
      browser(),
      GetServer()->GetURL("www.example2.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest,
                       ResetMediaFeed_WebContentsDestroyed) {
  DiscoverFeed();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

    // Fetch the feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(feeds[0]->id,
                                           run_loop.QuitClosure());
    run_loop.Run();
    WaitForDB();
  }

  // If we destroy the web contents then we should reset the feed.
  browser()->tab_strip_model()->CloseAllTabs();
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
  }
}

// Parameterized test to check that media feed discovery works with different
// header HTMLs.
class MediaFeedsDiscoveryBrowserTest
    : public MediaFeedsBrowserTest,
      public ::testing::WithParamInterface<TestData> {
 public:
  net::EmbeddedTestServer* GetServer() override {
    return GetParam().https ? MediaFeedsBrowserTest::GetServer()
                            : embedded_test_server();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override {
    if (request.relative_url == kMediaFeedsTestURL) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(base::StringPrintf(kMediaFeedsTestHTML,
                                               GetParam().head_html.c_str()));
      return response;
    }
    return nullptr;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaFeedsDiscoveryBrowserTest,
    ::testing::Values(
        TestData{"<link rel=feed type=\"application/ld+json\" "
                 "href=\"/test\"/>",
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

IN_PROC_BROWSER_TEST_P(MediaFeedsDiscoveryBrowserTest, Discover) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  DiscoverFeed();

  // Check we discovered the feed.
  std::set<GURL> expected_urls;

  if (GetParam().discovered)
    expected_urls.insert(GetServer()->GetURL("/test"));
  EXPECT_EQ(expected_urls, GetDiscoveredFeedURLs());

  // Check that we did/didn't record this to UKM.
  using Entry = ukm::builders::Media_Feed_Discover;
  auto entries = ukm_recorder.GetEntriesByName(Entry::kEntryName);

  if (GetParam().discovered) {
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntrySourceHasUrl(
        entries[0], GetServer()->GetURL(kMediaFeedsTestURL));
    ukm_recorder.ExpectEntryMetric(entries[0], Entry::kHasMediaFeedName, 1);
  } else {
    EXPECT_TRUE(entries.empty());
  }
}

}  // namespace media_feeds
