// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

#include <memory>
#include <vector>

#include "base/process/kill.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

using testing::_;
using testing::StrictMock;
using LoadingState = TabLoadTracker::LoadingState;

namespace {

std::unique_ptr<content::NavigationSimulator> NavigateAndWaitForResponse(
    content::WebContents* web_contents,
    const GURL& url) {
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  navigation->SetKeepLoading(true);
  navigation->Start();
  EXPECT_TRUE(web_contents->IsWaitingForResponse());
  return navigation;
}

std::unique_ptr<content::NavigationSimulator> NavigateAndKeepLoading(
    content::WebContents* web_contents,
    const GURL& url) {
  auto navigation = NavigateAndWaitForResponse(web_contents, url);
  navigation->Commit();
  EXPECT_FALSE(web_contents->IsWaitingForResponse());
  return navigation;
}

void NavigateAndFinishLoading(content::WebContents* web_contents,
                              const GURL& url) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents, url);
}

}  // namespace

// Test wrapper of TabLoadTracker that exposes some internals.
class TestTabLoadTracker : public TabLoadTracker {
 public:
  using TabLoadTracker::StartTracking;
  using TabLoadTracker::StopTracking;
  using TabLoadTracker::DidStartLoading;
  using TabLoadTracker::DidReceiveResponse;
  using TabLoadTracker::DidFailLoad;
  using TabLoadTracker::RenderProcessGone;
  using TabLoadTracker::OnPageAlmostIdle;
  using TabLoadTracker::DetermineLoadingState;

  TestTabLoadTracker() : all_tabs_are_non_ui_tabs_(false) {}
  virtual ~TestTabLoadTracker() {}

  // Some accessors for TabLoadTracker internals.
  const TabMap& tabs() const { return tabs_; }

  // Determines if the tab has been marked as having received the
  // DidStartLoading event.
  bool DidStartLoadingSeen(content::WebContents* web_contents) {
    auto it = tabs_.find(web_contents);
    if (it == tabs_.end())
      return false;
    return it->second.did_start_loading_seen;
  }

  bool IsUiTab(content::WebContents* web_contents) override {
    if (all_tabs_are_non_ui_tabs_)
      return false;
    return TabLoadTracker::IsUiTab(web_contents);
  }

  void SetAllTabsAreNonUiTabs(bool enabled) {
    all_tabs_are_non_ui_tabs_ = enabled;
  }

 private:
  bool all_tabs_are_non_ui_tabs_;
};

// A mock observer class.
class LenientMockObserver : public TabLoadTracker::Observer {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  // TabLoadTracker::Observer implementation:
  MOCK_METHOD2(OnStartTracking, void(content::WebContents*, LoadingState));
  MOCK_METHOD3(OnLoadingStateChange,
               void(content::WebContents*, LoadingState, LoadingState));
  MOCK_METHOD2(OnStopTracking, void(content::WebContents*, LoadingState));

 private:
  DISALLOW_COPY_AND_ASSIGN(LenientMockObserver);
};
using MockObserver = testing::StrictMock<LenientMockObserver>;

// A WebContentsObserver that forwards relevant WebContents events to the
// provided tracker.
class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  TestWebContentsObserver(content::WebContents* web_contents,
                          TestTabLoadTracker* tracker)
      : content::WebContentsObserver(web_contents), tracker_(tracker) {}

  ~TestWebContentsObserver() override {}

  // content::WebContentsObserver:
  void DidStartLoading() override { tracker_->DidStartLoading(web_contents()); }
  void DidReceiveResponse() override {
    tracker_->DidReceiveResponse(web_contents());
  }
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override {
    tracker_->DidFailLoad(web_contents());
  }
  void RenderProcessGone(base::TerminationStatus status) override {
    tracker_->RenderProcessGone(web_contents(), status);
  }

 private:
  TestTabLoadTracker* tracker_;
};

// The test harness.
class TabLoadTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    contents1_ = CreateTestWebContents();
    contents2_ = CreateTestWebContents();
    contents3_ = CreateTestWebContents();
    contents4_ = CreateTestWebContents();

    tracker_.AddObserver(&observer_);
  }

  void TearDown() override {
    // The WebContents must be deleted before the test harness deletes the
    // RenderProcessHost.
    contents1_.reset();
    contents2_.reset();
    contents3_.reset();
    contents4_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void ExpectTabCounts(size_t unloaded, size_t loading, size_t loaded) {
    size_t tabs = unloaded + loading + loaded;
    EXPECT_EQ(tabs, tracker().GetTabCount());
    EXPECT_EQ(unloaded, tracker().GetUnloadedTabCount());
    EXPECT_EQ(loading, tracker().GetLoadingTabCount());
    EXPECT_EQ(loaded, tracker().GetLoadedTabCount());
  }

  void ExpectUiTabCounts(size_t unloaded, size_t loading, size_t loaded) {
    size_t tabs = unloaded + loading + loaded;
    EXPECT_EQ(tabs, tracker().GetUiTabCount());
    EXPECT_EQ(unloaded, tracker().GetUnloadedUiTabCount());
    EXPECT_EQ(loading, tracker().GetLoadingUiTabCount());
    EXPECT_EQ(loaded, tracker().GetLoadedUiTabCount());
  }

  void StateTransitionsTest(bool use_non_ui_tabs, bool start_hidden);

  TestTabLoadTracker& tracker() { return tracker_; }
  MockObserver& observer() { return observer_; }

  content::WebContents* contents1() { return contents1_.get(); }
  content::WebContents* contents2() { return contents2_.get(); }
  content::WebContents* contents3() { return contents3_.get(); }
  content::WebContents* contents4() { return contents4_.get(); }

 private:
  TestTabLoadTracker tracker_;
  MockObserver observer_;

  std::unique_ptr<content::WebContents> contents1_;
  std::unique_ptr<content::WebContents> contents2_;
  std::unique_ptr<content::WebContents> contents3_;
  std::unique_ptr<content::WebContents> contents4_;
};

// A macro that ensures that a meaningful line number gets included in the
// stack trace when ExpectTabCounts fails.
#define EXPECT_TAB_COUNTS(b, c, d) \
  {                                \
    SCOPED_TRACE("");              \
    ExpectTabCounts(b, c, d);      \
  }
#define EXPECT_UI_TAB_COUNTS(b, c, d) \
  {                                   \
    SCOPED_TRACE("");                 \
    ExpectUiTabCounts(b, c, d);       \
  }
#define EXPECT_TAB_AND_UI_TAB_COUNTS(b, c, d) \
  {                                           \
    SCOPED_TRACE("");                         \
    ExpectTabCounts(b, c, d);                 \
    ExpectUiTabCounts(b, c, d);               \
  }

TEST_F(TabLoadTrackerTest, DetermineLoadingStateHiddenContents) {
  contents1()->WasHidden();

  EXPECT_EQ(LoadingState::UNLOADED,
            tracker().DetermineLoadingState(contents1()));

  // A hidden page that is loading and waiting for a response is UNLOADED.
  auto navigation =
      NavigateAndWaitForResponse(contents1(), GURL("http://chromium.org"));
  EXPECT_TRUE(contents1()->IsLoading());
  EXPECT_TRUE(contents1()->IsWaitingForResponse());
  EXPECT_EQ(LoadingState::UNLOADED,
            tracker().DetermineLoadingState(contents1()));

  // Simulate receiving a response and expect the page to be loading.
  navigation->Commit();
  EXPECT_TRUE(contents1()->IsLoading());
  EXPECT_FALSE(contents1()->IsWaitingForResponse());
  EXPECT_EQ(LoadingState::LOADING,
            tracker().DetermineLoadingState(contents1()));

  // Indicate that loading is finished and expect the state to transition.
  navigation->StopLoading();
  EXPECT_EQ(LoadingState::LOADED, tracker().DetermineLoadingState(contents1()));
}

TEST_F(TabLoadTrackerTest, DetermineLoadingStateVisibleContents) {
  EXPECT_EQ(LoadingState::UNLOADED,
            tracker().DetermineLoadingState(contents1()));

  contents1()->WasShown();

  // A visible page that is loading and waiting for a response is LOADING.
  auto navigation =
      NavigateAndWaitForResponse(contents1(), GURL("http://chromium.org"));
  EXPECT_TRUE(contents1()->IsLoading());
  EXPECT_TRUE(contents1()->IsWaitingForResponse());
  EXPECT_EQ(LoadingState::LOADING,
            tracker().DetermineLoadingState(contents1()));

  // Simulate receiving a response and expect the page to still be loading.
  navigation->Commit();
  EXPECT_EQ(LoadingState::LOADING,
            tracker().DetermineLoadingState(contents1()));

  // Indicate that loading is finished and expect the state to transition.
  navigation->StopLoading();
  EXPECT_EQ(LoadingState::LOADED, tracker().DetermineLoadingState(contents1()));
}

void TabLoadTrackerTest::StateTransitionsTest(bool use_non_ui_tabs,
                                              bool start_hidden) {
  tracker().SetAllTabsAreNonUiTabs(use_non_ui_tabs);

  int expected_unloaded = 0;
  int expected_loading = 0;
  int expected_loaded = 0;

  // Loading state of a contents that is waiting for a response.
  LoadingState waiting_for_response_loading_state;

  // Set up contents visibility.
  if (start_hidden) {
    contents1()->WasHidden();
    contents2()->WasHidden();
    contents3()->WasHidden();
    contents4()->WasHidden();
    waiting_for_response_loading_state = LoadingState::UNLOADED;
  } else {
    EXPECT_EQ(contents1()->GetVisibility(), content::Visibility::VISIBLE);
    EXPECT_EQ(contents2()->GetVisibility(), content::Visibility::VISIBLE);
    EXPECT_EQ(contents3()->GetVisibility(), content::Visibility::VISIBLE);
    EXPECT_EQ(contents4()->GetVisibility(), content::Visibility::VISIBLE);
    waiting_for_response_loading_state = LoadingState::LOADING;
  }

  // Set up contents in unloaded, waiting for response, response received and
  // loaded states. This tests each possible "entry" state.
  auto navigation_tab_2 =
      NavigateAndWaitForResponse(contents2(), GURL("http://foo.com"));
  auto navigation_tab_3 =
      NavigateAndKeepLoading(contents3(), GURL("http://bar.com"));
  NavigateAndFinishLoading(contents4(), GURL("http://baz.com"));

  // Add contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::UNLOADED));
  tracker().StartTracking(contents1());
  testing::Mock::VerifyAndClear(&observer());
  ++expected_unloaded;
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }

  EXPECT_CALL(observer(),
              OnStartTracking(contents2(), waiting_for_response_loading_state));
  tracker().StartTracking(contents2());
  testing::Mock::VerifyAndClear(&observer());
  if (start_hidden)
    ++expected_unloaded;
  else
    ++expected_loading;
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }

  EXPECT_CALL(observer(), OnStartTracking(contents3(), LoadingState::LOADING));
  tracker().StartTracking(contents3());
  testing::Mock::VerifyAndClear(&observer());
  ++expected_loading;
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents4(), LoadingState::LOADED));
  tracker().StartTracking(contents4());
  testing::Mock::VerifyAndClear(&observer());
  ++expected_loaded;
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start observers for the contents.
  TestWebContentsObserver observer1(contents1(), &tracker());
  TestWebContentsObserver observer2(contents2(), &tracker());
  TestWebContentsObserver observer3(contents3(), &tracker());
  TestWebContentsObserver observer4(contents4(), &tracker());

  // Now test all of the possible state transitions.

  // Finish the loading for contents3.
  navigation_tab_3->StopLoading();
  // The state transition should only occur *after* the PAI signal.
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }

  EXPECT_CALL(observer(),
              OnLoadingStateChange(contents3(), LoadingState::LOADING,
                                   LoadingState::LOADED));
  tracker().OnPageAlmostIdle(contents3());
  testing::Mock::VerifyAndClearExpectations(&observer());
  --expected_loading;
  ++expected_loaded;

  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Receive response for contents2.
  if (start_hidden) {
    EXPECT_CALL(observer(),
                OnLoadingStateChange(contents2(), LoadingState::UNLOADED,
                                     LoadingState::LOADING));
  }
  navigation_tab_2->Commit();
  testing::Mock::VerifyAndClearExpectations(&observer());
  if (start_hidden) {
    --expected_unloaded;
    ++expected_loading;
  }
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }

  // Start the loading for contents1.
  if (!start_hidden) {
    EXPECT_CALL(observer(),
                OnLoadingStateChange(contents1(), LoadingState::UNLOADED,
                                     LoadingState::LOADING));
    --expected_unloaded;
    ++expected_loading;
  }
  auto navigation_tab_1 =
      NavigateAndWaitForResponse(contents1(), GURL("http://baz.com"));
  testing::Mock::VerifyAndClearExpectations(&observer());
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }

  if (start_hidden) {
    EXPECT_CALL(observer(),
                OnLoadingStateChange(contents1(), LoadingState::UNLOADED,
                                     LoadingState::LOADING));
    --expected_unloaded;
    ++expected_loading;
  }
  navigation_tab_1->Commit();
  testing::Mock::VerifyAndClearExpectations(&observer());
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }

  testing::Mock::VerifyAndClearExpectations(&observer());

  // Stop the loading with an error. The tab should go back to a LOADED
  // state.
  EXPECT_CALL(observer(),
              OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                   LoadingState::LOADED));
  navigation_tab_1->FailLoading(GURL("http://baz.com"), 500,
                                base::UTF8ToUTF16("server error"));
  testing::Mock::VerifyAndClearExpectations(&observer());
  --expected_loading;
  ++expected_loaded;
  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Crash the render process corresponding to the main frame of a tab. This
  // should cause the tab to transition to the UNLOADED state.
  EXPECT_CALL(observer(),
              OnLoadingStateChange(contents1(), LoadingState::LOADED,
                                   LoadingState::UNLOADED));
  content::MockRenderProcessHost* rph =
      static_cast<content::MockRenderProcessHost*>(
          contents1()->GetMainFrame()->GetProcess());
  rph->SimulateCrash();
  testing::Mock::VerifyAndClearExpectations(&observer());
  --expected_loaded;
  ++expected_unloaded;

  if (use_non_ui_tabs) {
    EXPECT_TAB_COUNTS(expected_unloaded, expected_loading, expected_loaded);
    EXPECT_UI_TAB_COUNTS(0, 0, 0);
  } else {
    EXPECT_TAB_AND_UI_TAB_COUNTS(expected_unloaded, expected_loading,
                                 expected_loaded);
  }
}

TEST_F(TabLoadTrackerTest, StateTransitionsVisible) {
  StateTransitionsTest(false /* use_non_ui_tabs */, false /* start_hidden */);
}

TEST_F(TabLoadTrackerTest, StateTransitionsNonUiTabsVisible) {
  StateTransitionsTest(true /* use_non_ui_tabs */, false /* start_hidden */);
}

TEST_F(TabLoadTrackerTest, StateTransitionsHidden) {
  StateTransitionsTest(false /* use_non_ui_tabs */, true /* start_hidden */);
}

TEST_F(TabLoadTrackerTest, StateTransitionsNonUiTabsHidden) {
  StateTransitionsTest(true /* use_non_ui_tabs */, true /* start_hidden */);
}

TEST_F(TabLoadTrackerTest, PrerenderContentsDoesNotChangeUiTabCounts) {
  NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::LOADING));
  tracker().StartTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(0, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::UNLOADED));
  tracker().StartTracking(contents2());
  EXPECT_TAB_AND_UI_TAB_COUNTS(1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start observers for the contents.
  TestWebContentsObserver observer1(contents1(), &tracker());
  TestWebContentsObserver observer2(contents2(), &tracker());

  // Prerender some contents.
  prerender::test_utils::RestorePrerenderMode restore_prerender_mode;
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::DEPRECATED_PRERENDER_MODE_ENABLED);
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile());
  GURL url("http://www.example.com");
  const gfx::Size kSize(640, 480);
  std::unique_ptr<prerender::PrerenderHandle> prerender_handle(
      prerender_manager->AddPrerenderFromOmnibox(
          url, contents1()->GetController().GetDefaultSessionStorageNamespace(),
          kSize));
  const std::vector<content::WebContents*> contentses =
      prerender_manager->GetAllPrerenderingContents();
  ASSERT_EQ(1U, contentses.size());

  // Check that the visibility and loading state for a prerender WebContents is
  // as previously observed. If prerender logic changes, this should be updated.
  EXPECT_EQ(content::Visibility::VISIBLE, contentses[0]->GetVisibility());
  EXPECT_TRUE(contentses[0]->IsLoading());
  EXPECT_TRUE(contentses[0]->IsWaitingForResponse());

  // Prerendering should not change the UI tab counts, but should increase
  // overall tab count. Note, contentses[0] is UNLOADED since it is not a test
  // web contents and therefore hasn't started receiving data.
  TestWebContentsObserver prerender_observer(contentses[0], &tracker());
  EXPECT_CALL(observer(),
              OnStartTracking(contentses[0], LoadingState::LOADING));
  tracker().StartTracking(contentses[0]);
  EXPECT_TAB_COUNTS(1, 2, 0);
  EXPECT_UI_TAB_COUNTS(1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  prerender_manager->CancelAllPrerenders();
}

TEST_F(TabLoadTrackerTest, SwapInUiTabContents) {
  NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::LOADING));
  tracker().StartTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(0, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::UNLOADED));
  tracker().StartTracking(contents2());
  EXPECT_TAB_AND_UI_TAB_COUNTS(1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Start observers for the contents.
  TestWebContentsObserver observer1(contents1(), &tracker());
  TestWebContentsObserver observer2(contents2(), &tracker());

  // Simulate non-ui tab contents running in the background and getting swapped
  // in. Non-ui tabs should not change the ui tab counts, but should change the
  // overall tab counts.
  std::unique_ptr<content::WebContents> non_ui_tab_contents =
      CreateTestWebContents();
  EXPECT_CALL(observer(), OnStartTracking(non_ui_tab_contents.get(),
                                          LoadingState::UNLOADED));
  tracker().SetAllTabsAreNonUiTabs(true);
  tracker().StartTracking(non_ui_tab_contents.get());
  EXPECT_TAB_COUNTS(2, 1, 0);
  EXPECT_UI_TAB_COUNTS(1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());
  // Swap in the prerender contents and simulate resulting tab strip swap.
  // |non_ui_tab_contents| is already being tracked. The UI tab count should
  // remain stable through the swap.
  EXPECT_CALL(observer(), OnStopTracking(contents1(), LoadingState::LOADING));
  tracker().SetAllTabsAreNonUiTabs(false);
  tracker().SwapTabContents(contents1(), non_ui_tab_contents.get());
  // After swap, but before we stop tracking the swapped-out contents. The UI
  // tab counts should be in the end-state, but the total tab counts will be in
  // the pre-swap state while the swapped-out contents is still being tracked.
  EXPECT_TAB_COUNTS(2, 1, 0);
  EXPECT_UI_TAB_COUNTS(2, 0, 0);
  tracker().StopTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(2, 0, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(TabLoadTrackerTest, SwapInUntrackedContents) {
  NavigateAndKeepLoading(contents1(), GURL("http://baz.com"));

  // Add the contents to the tracker.
  EXPECT_CALL(observer(), OnStartTracking(contents1(), LoadingState::LOADING));
  tracker().StartTracking(contents1());
  EXPECT_TAB_AND_UI_TAB_COUNTS(0, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(observer(), OnStartTracking(contents2(), LoadingState::UNLOADED));
  tracker().StartTracking(contents2());
  EXPECT_TAB_AND_UI_TAB_COUNTS(1, 1, 0);
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Create an untracked web contents in the UNLOADED state, and swap it with
  // the contents in the LOADING state. Since |untracked_contents| has no tab
  // helper attached, swapping it in shouldn't changed the tab count.
  std::unique_ptr<content::WebContents> untracked_contents =
      CreateTestWebContents();
  tracker().SwapTabContents(contents1(), untracked_contents.get());
  // The total counts will remain stable since swapping out doesn't cause any
  // web contents to stop being tracking. However, the swapped-out contents are
  // no longer included in UI tab counts, and the swapped-in contents won't be
  // until it is tracked.
  EXPECT_TAB_COUNTS(1, 1, 0);
  EXPECT_UI_TAB_COUNTS(1, 0, 0);

  // Simulate swap in tab strip, which would cause |untracked_contents| to be
  // tracked and the tab counts to change.
  EXPECT_CALL(observer(), OnStopTracking(contents1(), LoadingState::LOADING));
  EXPECT_CALL(observer(), OnStartTracking(untracked_contents.get(),
                                          LoadingState::UNLOADED));
  tracker().StopTracking(contents1());
  tracker().StartTracking(untracked_contents.get());
  EXPECT_TAB_AND_UI_TAB_COUNTS(2, 0, 0);
}

}  // namespace resource_coordinator
