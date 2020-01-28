// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#include "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#import "ios/chrome/test/fakes/fake_web_state_list_observing_delegate.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/web_state_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kURL1[] = "https://www.some.url.com";
const char kURL2[] = "https://www.some.url2.com";

class TabModelTest : public PlatformTest {
 public:
  TabModelTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
        web_client_(std::make_unique<ChromeWebClient>()),
        web_state_list_delegate_(
            std::make_unique<BrowserWebStateListDelegate>()),
        web_state_list_(
            std::make_unique<WebStateList>(web_state_list_delegate_.get())) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list_.get());
    // Web usage is disabled during these tests.
    web_usage_enabler_ =
        WebStateListWebUsageEnablerFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get());
    web_usage_enabler_->SetWebUsageEnabled(false);

    session_window_ = [[SessionWindowIOS alloc] init];

    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());

    agent_ = TabInsertionBrowserAgent::FromBrowser(browser_.get());
    session_service_ = [[TestSessionService alloc] init];
    // Create session restoration agent with just a dummy session
    // service so the async state saving doesn't trigger unless actually
    // wanted.

    SessionRestorationBrowserAgent::CreateForBrowser(browser_.get(),
                                                     session_service_);
    SetTabModel(CreateTabModel(nil));
  }

  ~TabModelTest() override = default;

  void TearDown() override {
    SetTabModel(nil);
    PlatformTest::TearDown();
  }

  void SetTabModel(TabModel* tab_model) {
    if (tab_model_) {
      web_usage_enabler_->SetWebStateList(nullptr);
      @autoreleasepool {
        [tab_model_ disconnect];
        tab_model_ = nil;
      }
    }

    tab_model_ = tab_model;
    web_usage_enabler_->SetWebStateList(web_state_list_.get());
  }

  TabModel* CreateTabModel(SessionWindowIOS* session_window) {
    TabModel* tab_model([[TabModel alloc] initWithBrowser:browser_.get()]);
    [tab_model restoreSessionWindow:session_window forInitialRestore:YES];
    [tab_model setPrimary:YES];
    return tab_model;
  }

  const web::NavigationManager::WebLoadParams Params(GURL url) {
    return Params(url, ui::PAGE_TRANSITION_TYPED);
  }

  const web::NavigationManager::WebLoadParams Params(
      GURL url,
      ui::PageTransition transition) {
    web::NavigationManager::WebLoadParams loadParams(url);
    loadParams.referrer = web::Referrer();
    loadParams.transition_type = transition;
    return loadParams;
  }

 protected:
  // Creates a session window with entries named "restored window 1",
  // "restored window 2" and "restored window 3" and the second entry
  // marked as selected.
  SessionWindowIOS* CreateSessionWindow() {
    NSMutableArray<CRWSessionStorage*>* sessions = [NSMutableArray array];
    for (int i = 0; i < 3; i++) {
      CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
      session_storage.lastCommittedItemIndex = -1;
      [sessions addObject:session_storage];
    }
    return [[SessionWindowIOS alloc] initWithSessions:sessions selectedIndex:1];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;

  std::unique_ptr<WebStateList> web_state_list_;
  std::unique_ptr<Browser> browser_;
  SessionWindowIOS* session_window_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  TabInsertionBrowserAgent* agent_;
  TestSessionService* session_service_;
  WebStateListWebUsageEnabler* web_usage_enabler_;
  TabModel* tab_model_;
};

TEST_F(TabModelTest, IsEmpty) {
  EXPECT_EQ(web_state_list_->count(), 0);
  EXPECT_TRUE([tab_model_ isEmpty]);
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/0,
                         /*in_background=*/false);
  ASSERT_EQ(1, web_state_list_->count());
  EXPECT_FALSE([tab_model_ isEmpty]);
}

TEST_F(TabModelTest, CloseTabAtIndexBeginning) {
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/web_state_list_->count(),
                         /*in_background=*/false);
  web::WebState* web_state1 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false);
  web::WebState* web_state2 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false);
  [tab_model_ closeTabAtIndex:0];

  ASSERT_EQ(2, web_state_list_->count());
  EXPECT_EQ(web_state1, web_state_list_->GetWebStateAt(0));
  EXPECT_EQ(web_state2, web_state_list_->GetWebStateAt(1));
}

TEST_F(TabModelTest, CloseTabAtIndexMiddle) {
  web::WebState* web_state0 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false);
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/web_state_list_->count(),
                         /*in_background=*/false);
  web::WebState* web_state2 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false);

  [tab_model_ closeTabAtIndex:1];

  ASSERT_EQ(2, web_state_list_->count());
  EXPECT_EQ(web_state0, web_state_list_->GetWebStateAt(0));
  EXPECT_EQ(web_state2, web_state_list_->GetWebStateAt(1));
}

TEST_F(TabModelTest, CloseTabAtIndexLast) {
  web::WebState* web_state0 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false);
  web::WebState* web_state1 =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false);
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/web_state_list_->count(),
                         /*in_background=*/false);

  [tab_model_ closeTabAtIndex:2];

  ASSERT_EQ(2, web_state_list_->count());
  EXPECT_EQ(web_state0, web_state_list_->GetWebStateAt(0));
  EXPECT_EQ(web_state1, web_state_list_->GetWebStateAt(1));
}

TEST_F(TabModelTest, CloseTabAtIndexOnlyOne) {
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/web_state_list_->count(),
                         /*in_background=*/false);

  [tab_model_ closeTabAtIndex:0];
  EXPECT_EQ(0, web_state_list_->count());
}

// TODO(crbug.com/888674): migrate this to EG test so it can be tested with
// WKBasedNavigationManager.
TEST_F(TabModelTest, DISABLED_RestoreSessionOnNTPTest) {
  web::WebState* web_state = agent_->InsertWebState(Params(GURL(kURL1)),
                                                    /*parent=*/nil,
                                                    /*opened_by_dom=*/false,
                                                    /*index=*/0,
                                                    /*in_background=*/false);
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state);

  // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
  id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state, delegate);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window forInitialRestore:NO];

  ASSERT_EQ(3, web_state_list_->count());
  EXPECT_EQ(web_state_list_->GetWebStateAt(1),
            web_state_list_->GetActiveWebState());
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(0));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(1));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(2));
}

// TODO(crbug.com/888674): migrate this to EG test so it can be tested with
// WKBasedNavigationManager.
TEST_F(TabModelTest, DISABLED_RestoreSessionOn2NtpTest) {
  web::WebState* web_state0 = agent_->InsertWebState(Params(GURL(kURL1)),
                                                     /*parent=*/nil,
                                                     /*opened_by_dom=*/false,
                                                     /*index=*/0,
                                                     /*in_background=*/false);
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state0);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();
  web::WebState* web_state1 = agent_->InsertWebState(Params(GURL(kURL1)),
                                                     /*parent=*/nil,
                                                     /*opened_by_dom=*/false,
                                                     /*index=*/0,
                                                     /*in_background=*/false);
  web_state_impl = static_cast<web::WebStateImpl*>(web_state1);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window forInitialRestore:NO];

  ASSERT_EQ(5, web_state_list_->count());
  EXPECT_EQ(web_state_list_->GetWebStateAt(3),
            web_state_list_->GetActiveWebState());
  EXPECT_EQ(web_state0, web_state_list_->GetWebStateAt(0));
  EXPECT_EQ(web_state1, web_state_list_->GetWebStateAt(1));
  EXPECT_NE(web_state0, web_state_list_->GetWebStateAt(2));
  EXPECT_NE(web_state0, web_state_list_->GetWebStateAt(3));
  EXPECT_NE(web_state0, web_state_list_->GetWebStateAt(4));
  EXPECT_NE(web_state1, web_state_list_->GetWebStateAt(2));
  EXPECT_NE(web_state1, web_state_list_->GetWebStateAt(3));
  EXPECT_NE(web_state1, web_state_list_->GetWebStateAt(4));
}

// TODO(crbug.com/888674): migrate this to EG test so it can be tested with
// WKBasedNavigationManager.
TEST_F(TabModelTest, DISABLED_RestoreSessionOnAnyTest) {
  web::WebState* web_state = agent_->InsertWebState(Params(GURL(kURL1)),
                                                    /*parent=*/nil,
                                                    /*opened_by_dom=*/false,
                                                    /*index=*/0,
                                                    /*in_background=*/false);
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window forInitialRestore:NO];

  ASSERT_EQ(4, web_state_list_->count());
  EXPECT_EQ(web_state_list_->GetWebStateAt(2),
            web_state_list_->GetActiveWebState());
  EXPECT_EQ(web_state, web_state_list_->GetWebStateAt(0));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(1));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(2));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(3));
}

TEST_F(TabModelTest, CloseAllTabs) {
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/web_state_list_->count(),
                         /*in_background=*/false);
  agent_->InsertWebState(Params(GURL(kURL2)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/web_state_list_->count(),
                         /*in_background=*/false);
  agent_->InsertWebState(Params(GURL(kURL1)),
                         /*parent=*/nil,
                         /*opened_by_dom=*/false,
                         /*index=*/web_state_list_->count(),
                         /*in_background=*/false);
  [tab_model_ closeAllTabs];

  EXPECT_EQ(0, web_state_list_->count());
}

TEST_F(TabModelTest, CloseAllTabsWithNoTabs) {
  [tab_model_ closeAllTabs];

  EXPECT_EQ(0, web_state_list_->count());
}

TEST_F(TabModelTest, InsertWithSessionController) {
  EXPECT_EQ(web_state_list_->count(), 0);
  EXPECT_TRUE([tab_model_ isEmpty]);

  web::WebState* new_web_state =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false);

  EXPECT_EQ(web_state_list_->count(), 1);
  EXPECT_EQ(new_web_state, web_state_list_->GetWebStateAt(0));
  web_state_list_->ActivateWebStateAt(0);
  web::WebState* current_web_state = web_state_list_->GetActiveWebState();
  EXPECT_TRUE(current_web_state);
}

}  // anonymous namespace
