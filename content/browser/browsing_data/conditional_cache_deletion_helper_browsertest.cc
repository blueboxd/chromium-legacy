// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/browsing_data/conditional_cache_deletion_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

class ConditionalCacheDeletionHelperBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {}

  void CreateCacheEntry(const std::set<GURL>& urls) {
    for (auto& url : urls) {
      ASSERT_EQ(net::OK, LoadBasicRequest(
                             storage_partition()->GetNetworkContext(), url));
    }
  }

  bool TestCacheEntry(const GURL& url) {
    return LoadBasicRequest(storage_partition()->GetNetworkContext(), url,
                            0 /* process_id */, 0 /* render_frame_id */,
                            net::LOAD_ONLY_FROM_CACHE |
                                net::LOAD_SKIP_CACHE_VALIDATION) == net::OK;
  }

  void CompareRemainingKeys(const std::set<GURL>& expected_urls,
                            const std::set<GURL>& erase_urls) {
    for (auto& url : expected_urls)
      EXPECT_TRUE(TestCacheEntry(url));
    for (auto& url : erase_urls)
      EXPECT_FALSE(TestCacheEntry(url));
  }

  void DoneCallback(int value) {
    DCHECK_GE(value, 0);  // Negative values represent an error.
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    waitable_event_->Signal();
  }

  void WaitForTasksOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    waitable_event_->Wait();
  }

  StoragePartition* storage_partition() {
    return BrowserContext::GetDefaultStoragePartition(browser_context());
  }

 private:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  base::OnceCallback<void(int)> done_callback_;
  std::unique_ptr<base::WaitableEvent> waitable_event_;
};

// Tests that ConditionalCacheDeletionHelper only deletes those cache entries
// that match the condition.
// Disabled on Android due to flakiness. See https://crbug.com/978891.
#if defined(OS_ANDROID)
#define MAYBE_Condition DISABLED_Condition
#else
#define MAYBE_Condition Condition
#endif
IN_PROC_BROWSER_TEST_F(ConditionalCacheDeletionHelperBrowserTest,
                       MAYBE_Condition) {
  std::set<GURL> urls = {
      embedded_test_server()->GetURL("foo.com", "/title1.html"),
      embedded_test_server()->GetURL("bar.com", "/title1.html"),
      embedded_test_server()->GetURL("baz.com", "/title1.html"),
      embedded_test_server()->GetURL("qux.com", "/title1.html")};

  CreateCacheEntry(urls);

  std::set<GURL> erase_urls = {
      embedded_test_server()->GetURL("bar.com", "/title1.html"),
      embedded_test_server()->GetURL("baz.com", "/title1.html"),
  };

  for (auto& url : erase_urls)
    urls.erase(url);

  network::mojom::ClearDataFilterPtr filter =
      network::mojom::ClearDataFilter::New();
  filter->type = network::mojom::ClearDataFilter::Type::DELETE_MATCHES;
  for (auto& url : erase_urls)
    filter->origins.push_back(url::Origin::Create(url));

  base::RunLoop run_loop;
  storage_partition()->GetNetworkContext()->ClearHttpCache(
      base::Time(), base::Time::Max(), std::move(filter),
      run_loop.QuitClosure());
  run_loop.Run();

  CompareRemainingKeys(urls, erase_urls);
}

// Tests that ConditionalCacheDeletionHelper correctly constructs a condition
// for time and URL.
//
// Note: This test depends on the timing in cache backends and can be flaky
// if those backends are slow.
//
// It previously flaked on Mac 10.11 (crbug.com/646119) and on Linux/ChromeOS
// (crbug.com/624836) but it seems to be stable now.

// Disabled on Android due to flakiness. See https://crbug.com/978891.
#if defined(OS_ANDROID)
#define MAYBE_TimeAndURL DISABLED_TimeAndURL
#else
// https://crbug.com/911171: this test depends on the timing of the cache,
// which changes if it's running out-of-process.
#define MAYBE_TimeAndURL DISABLED_TimeAndURL
#endif
IN_PROC_BROWSER_TEST_F(ConditionalCacheDeletionHelperBrowserTest,
                       MAYBE_TimeAndURL) {
  // Create some entries.
  std::set<GURL> urls = {
      embedded_test_server()->GetURL("foo.com", "/title1.html"),
      embedded_test_server()->GetURL("example.com", "/title1.html"),
      embedded_test_server()->GetURL("bar.com", "/title1.html")};
  CreateCacheEntry(urls);

  // Wait some milliseconds for the cache to write the entries.
  // This assures that future entries will have timestamps strictly greater than
  // the ones we just added.
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  base::Time now = base::Time::Now();

  // Create a few more entries with a later timestamp.
  std::set<GURL> newer_urls = {
      embedded_test_server()->GetURL("foo.com", "/simple_page.html"),
      embedded_test_server()->GetURL("example.com", "/title2.html"),
      embedded_test_server()->GetURL("example.com", "/title3.html"),
      embedded_test_server()->GetURL("example2.com", "/simple_page.html")};
  CreateCacheEntry(newer_urls);

  // Create a condition for entries with the "http://example.com" origin
  // created after waiting.
  network::mojom::ClearDataFilterPtr filter =
      network::mojom::ClearDataFilter::New();
  filter->type = network::mojom::ClearDataFilter::Type::DELETE_MATCHES;
  filter->origins.push_back(
      url::Origin::Create(embedded_test_server()->GetURL("example.com", "/")));

  base::RunLoop run_loop;
  storage_partition()->GetNetworkContext()->ClearHttpCache(
      now, base::Time::Max(), std::move(filter), run_loop.QuitClosure());
  run_loop.Run();

  // Expect that only "title2.html" and "title3.html" were deleted.
  std::set<GURL> erase_urls = {
      embedded_test_server()->GetURL("example.com", "/title2.html"),
      embedded_test_server()->GetURL("example.com", "/title3.html"),
  };

  for (auto& url : newer_urls)
    urls.insert(url);

  for (auto& url : erase_urls)
    urls.erase(url);

  CompareRemainingKeys(urls, erase_urls);
}

}  // namespace content
