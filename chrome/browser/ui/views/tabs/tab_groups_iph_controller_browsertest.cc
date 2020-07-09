// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_groups_iph_controller.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Ref;
using ::testing::Return;

class TabGroupsIPHControllerBrowserTest : public DialogBrowserTest {
 public:
  TabGroupsIPHControllerBrowserTest() {
    service_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(RegisterMockTrackerFactory));
  }

 protected:
  void ShowUi(const std::string& name) override {
    OpenTabsToTrigger(browser());
  }

  void OpenTabsToTrigger(Browser* browser) {
    // We need to have 6 tabs to trigger our IPH. Browser tests start
    // with one tab, so open 5 more.
    for (int i = 0; i < 5; ++i)
      chrome::NewTab(browser);
  }

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser()->profile()));
  }

  feature_engagement::test::MockTracker* mock_tracker_;

 private:
  static void RegisterMockTrackerFactory(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext*) {
          auto mock_tracker =
              std::make_unique<feature_engagement::test::MockTracker>();

          // Other features may call into the backend.
          EXPECT_CALL(*mock_tracker, NotifyEvent(_)).Times(AnyNumber());
          EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
              .Times(AnyNumber())
              .WillRepeatedly(Return(false));

          return std::unique_ptr<KeyedService>(mock_tracker.release());
        }));
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      service_manager_subscription_;
};

IN_PROC_BROWSER_TEST_F(TabGroupsIPHControllerBrowserTest, InvokeUi_default) {
  // Allow the controller to show the promo.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(
                  Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1)
      .WillOnce(Return(true));

  // Expect the controller to notify on dismissal.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1);

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(TabGroupsIPHControllerBrowserTest,
                       HandlesBrowserShutdown) {
  Browser* second_browser = CreateBrowser(browser()->profile());

  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(
                  Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1);

  OpenTabsToTrigger(second_browser);
  CloseBrowserSynchronously(second_browser);
}
