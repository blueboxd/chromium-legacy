// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_fetcher_impl.h"

#include "base/run_loop.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "ui/gfx/image/image.h"
#include "weblayer/browser/favicon/favicon_fetcher_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl_factory.h"
#include "weblayer/browser/favicon/favicon_service_impl_observer.h"
#include "weblayer/browser/favicon/test_favicon_fetcher_delegate.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/favicon_fetcher_delegate.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
namespace {

// FaviconServiceImplObserver used to wait for download to fail.
class TestFaviconServiceImplObserver : public FaviconServiceImplObserver {
 public:
  void Wait() {
    ASSERT_EQ(nullptr, run_loop_.get());
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // FaviconServiceImplObserver:
  void OnUnableToDownloadFavicon() override {
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

using FaviconFetcherBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(FaviconFetcherBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page_with_favicon.html"),
      shell());
  fetcher_delegate.WaitForFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(fetcher_delegate.last_image(), fetcher->GetFavicon());
  EXPECT_EQ(1, fetcher_delegate.on_favicon_changed_call_count());
  fetcher_delegate.ClearLastImage();

  const GURL url2 =
      embedded_test_server()->GetURL("/simple_page_with_favicon2.html");
  shell()->tab()->GetNavigationController()->Navigate(url2);
  // Favicon doesn't change immediately on navigation.
  EXPECT_FALSE(fetcher->GetFavicon().IsEmpty());
  // Favicon does change once start is received.
  TestNavigationObserver test_observer(
      url2, TestNavigationObserver::NavigationEvent::kStart, shell());
  test_observer.Wait();
  EXPECT_TRUE(fetcher_delegate.last_image().IsEmpty());

  // Wait for new favicon.
  fetcher_delegate.WaitForFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(fetcher_delegate.last_image(), fetcher->GetFavicon());
  EXPECT_EQ(1, fetcher_delegate.on_favicon_changed_call_count());
}

IN_PROC_BROWSER_TEST_F(FaviconFetcherBrowserTest, NavigateToPageWithNoFavicon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page_with_favicon.html"),
      shell());
  fetcher_delegate.WaitForFavicon();
  fetcher_delegate.ClearLastImage();

  TestFaviconServiceImplObserver test_observer;
  FaviconServiceImplFactory::GetForProfile(
      static_cast<TabImpl*>(shell()->tab())->profile())
      ->set_observer(&test_observer);

  const GURL url2 = embedded_test_server()->GetURL("/simple_page.html");
  shell()->tab()->GetNavigationController()->Navigate(url2);
  EXPECT_TRUE(fetcher_delegate.last_image().IsEmpty());
  // Wait for the image load to fail.
  test_observer.Wait();
  EXPECT_TRUE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(0, fetcher_delegate.on_favicon_changed_call_count());
}

IN_PROC_BROWSER_TEST_F(FaviconFetcherBrowserTest,
                       ContentFaviconDriverLifetime) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      static_cast<TabImpl*>(shell()->tab())->web_contents();

  // Initially there should be no driver (because favicons haven't been
  // requested).
  EXPECT_EQ(nullptr,
            favicon::ContentFaviconDriver::FromWebContents(web_contents));

  // Request a fetcher, which should trigger creating ContentFaviconDriver.
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  EXPECT_NE(nullptr,
            favicon::ContentFaviconDriver::FromWebContents(web_contents));

  // Destroy the fetcher, which should destroy ContentFaviconDriver.
  fetcher.reset();
  EXPECT_EQ(nullptr,
            favicon::ContentFaviconDriver::FromWebContents(web_contents));

  // One more time, and this time navigate.
  fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page_with_favicon.html"),
      shell());
  fetcher_delegate.WaitForFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
}

}  // namespace weblayer
