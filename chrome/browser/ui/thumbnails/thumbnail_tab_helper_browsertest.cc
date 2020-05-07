// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/tab_loader_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/tab_loader.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

namespace {

class ThumbnailWaiter : public ThumbnailImage::Observer {
 public:
  ThumbnailWaiter() = default;
  ~ThumbnailWaiter() override = default;

  base::Optional<gfx::ImageSkia> WaitForThumbnail(ThumbnailImage* thumbnail) {
    DCHECK(!thumbnail_);
    thumbnail_ = thumbnail;
    scoped_observer_.Add(thumbnail);
    thumbnail_->RequestThumbnailImage();
    run_loop_.Run();
    return image_;
  }

 protected:
  // ThumbnailImage::Observer:
  void OnThumbnailImageAvailable(gfx::ImageSkia thumbnail_image) override {
    if (thumbnail_) {
      scoped_observer_.Remove(thumbnail_);
      thumbnail_ = nullptr;
      image_ = thumbnail_image;
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  ThumbnailImage* thumbnail_ = nullptr;
  base::Optional<gfx::ImageSkia> image_;
  ScopedObserver<ThumbnailImage, ThumbnailImage::Observer> scoped_observer_{
      this};
};

}  // anonymous namespace

// Test fixture for testing interaction of thumbnail tab helper and browser,
// specifically testing interaction of tab load and thumbnail capture.
class ThumbnailTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  ThumbnailTabHelperBrowserTest() {
    url1_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot1.html"));
    url2_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot2.html"));
  }

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  void SetMaxSimultaneousLoadsForTesting(TabLoader* tab_loader) {
    TabLoaderTester tester(tab_loader);
    tester.SetMaxSimultaneousLoadsForTesting(1);
  }
#endif

 protected:
  void SetUp() override {
    // This flag causes the thumbnail tab helper system to engage. Otherwise
    // there is no ThumbnailTabHelper created. Note that there *are* other flags
    // that also trigger the existence of the helper.
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCardImages);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    active_browser_list_ = BrowserList::GetInstance();
  }

  Browser* GetBrowser(int index) {
    CHECK(static_cast<int>(active_browser_list_->size()) > index);
    return active_browser_list_->get(index);
  }

  // Adds tabs to the given browser, all navigated to url1_. Returns
  // the final number of tabs.
  int AddSomeTabs(Browser* browser, int how_many) {
    int starting_tab_count = browser->tab_strip_model()->count();

    for (int i = 0; i < how_many; ++i) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url1_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    int tab_count = browser->tab_strip_model()->count();
    EXPECT_EQ(starting_tab_count + how_many, tab_count);
    return tab_count;
  }

  void EnsureTabLoaded(content::WebContents* tab) {
    content::NavigationController* controller = &tab->GetController();
    if (!controller->NeedsReload() && !controller->GetPendingEntry() &&
        !controller->GetWebContents()->IsLoading())
      return;

    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(controller));
    observer.Wait();
  }

  void WaitForAndVerifyThumbnail(Browser* browser, int tab_index) {
    auto* const web_contents = browser->tab_strip_model()->GetWebContentsAt(1);
    auto* const thumbnail_tab_helper =
        ThumbnailTabHelper::FromWebContents(web_contents);
    auto thumbnail = thumbnail_tab_helper->thumbnail();
    EXPECT_FALSE(thumbnail->has_data())
        << " tab at index " << tab_index << " already has data.";

    ThumbnailWaiter waiter;
    const base::Optional<gfx::ImageSkia> data =
        waiter.WaitForThumbnail(thumbnail.get());
    EXPECT_TRUE(thumbnail->has_data())
        << " tab at index " << tab_index << " thumbnail has no data.";
    ASSERT_TRUE(data) << " observer for tab at index " << tab_index
                      << " received no thumbnail.";
    EXPECT_FALSE(data->isNull())
        << " tab at index " << tab_index << " generated empty thumbnail.";
  }

  GURL url1_;
  GURL url2_;

  const BrowserList* active_browser_list_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailTabHelperBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ThumbnailTabHelperBrowserTest,
                       TabLoadTriggersScreenshot) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  DCHECK_EQ(2, browser()->tab_strip_model()->count());
  WaitForAndVerifyThumbnail(browser(), 1);
}

// TabLoader (used here) is available only when browser is built
// with ENABLE_SESSION_SERVICE.
#if BUILDFLAG(ENABLE_SESSION_SERVICE)

// Note that this code is mostly cribbed from the test:
//   TabRestoreTest.TabsFromRestoredWindowsAreLoadedGradually
// because we need to simulate a browser restore that doesn't reload all of the
// tabs - and then ensure that we do get thumbnails when we watch the tabs that
// didn't load.
IN_PROC_BROWSER_TEST_F(
    ThumbnailTabHelperBrowserTest,
    WatchingThumbnailAfterBrowserRestoreCausesPageLoadAndProducesThumbnail) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  Browser* browser2 = GetBrowser(1);

  // Add tabs and close browser.
  constexpr int kTabCount = 4;
  AddSomeTabs(browser2, kTabCount - browser2->tab_strip_model()->count());
  EXPECT_EQ(kTabCount, browser2->tab_strip_model()->count());
  CloseBrowserSynchronously(browser2);

  // When the tab loader is created configure it for this test. This ensures
  // that no more than 1 loading slot is used for the test.
  base::RepeatingCallback<void(TabLoader*)> callback = base::BindRepeating(
      &ThumbnailTabHelperBrowserTest::SetMaxSimultaneousLoadsForTesting,
      base::Unretained(this));
  TabLoaderTester::SetConstructionCallbackForTesting(&callback);

  // Restore recently closed window.
  chrome::OpenWindowWithRestoredTabs(browser()->profile());
  ASSERT_EQ(2U, active_browser_list_->size());
  browser2 = GetBrowser(1);

  EXPECT_EQ(kTabCount, browser2->tab_strip_model()->count());
  EXPECT_EQ(kTabCount - 1, browser2->tab_strip_model()->active_index());
  // These two tabs should be loaded by TabLoader.
  EnsureTabLoaded(browser2->tab_strip_model()->GetWebContentsAt(kTabCount - 1));
  EnsureTabLoaded(browser2->tab_strip_model()->GetWebContentsAt(0));

  // The following isn't necessary but just to be sure there is no any async
  // task that could have an impact on the expectations below. Note that because
  // we're on the browser main thread, tasks are executed in order, so this
  // effectively flushes the queue.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();

  // These tabs shouldn't want to be loaded.
  for (int tab_idx = 1; tab_idx < kTabCount - 1; ++tab_idx) {
    auto* contents = browser2->tab_strip_model()->GetWebContentsAt(tab_idx);
    EXPECT_FALSE(contents->IsLoading());
    EXPECT_TRUE(contents->GetController().NeedsReload());
  }

  // So we now know that tabs 1 and 2 are not [yet] loading.
  // See if the act of observing one causes the thumbnail to be generated.
  WaitForAndVerifyThumbnail(browser2, 1);

  // Clean up the callback.
  TabLoaderTester::SetConstructionCallbackForTesting(nullptr);
}

#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
