// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_groups_iph_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_controller_views.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

class TabGroupsIPHControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                profile()));

    promo_controller_ = browser_view()->feature_promo_controller();
    iph_controller_ = browser_view()->tab_groups_iph_controller();
  }

  void TearDown() override {
    iph_controller_ = nullptr;
    promo_controller_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(feature_engagement::TrackerFactory::GetInstance(),
                           base::BindRepeating(MakeTestTracker));
    return factories;
  }

 private:
  static std::unique_ptr<KeyedService> MakeTestTracker(
      content::BrowserContext* context) {
    auto tracker =
        std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

    // Allow other code to call into the tracker.
    EXPECT_CALL(*tracker, NotifyEvent(_)).Times(AnyNumber());
    EXPECT_CALL(*tracker, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    return tracker;
  }

 protected:
  NiceMock<feature_engagement::test::MockTracker>* mock_tracker_;
  FeaturePromoController* promo_controller_;
  TabGroupsIPHController* iph_controller_;
};

TEST_F(TabGroupsIPHControllerTest, NotifyEventAndTriggerOnSixthTabOpened) {
  // TabGroupsIPHController shouldn't issue any calls...yet
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::kSixthTabOpened))
      .Times(0);
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(
                  Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(0);

  for (int i = 0; i < 5; ++i)
    chrome::NewTab(browser());

  // Upon opening a sixth tab, our controller should both notify the IPH
  // backend and ask to trigger IPH.
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::kSixthTabOpened))
      .Times(1);
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(
                  Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1)
      .WillOnce(Return(false));
  chrome::NewTab(browser());
}

TEST_F(TabGroupsIPHControllerTest, NotifyEventOnTabGroupCreated) {
  // Creating an ungrouped tab shouldn't do anything.
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::kTabGroupCreated))
      .Times(0);

  chrome::NewTab(browser());

  // Adding the tab to a new group should issue the relevant event.
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::kTabGroupCreated))
      .Times(1);

  browser()->tab_strip_model()->AddToNewGroup({0});
}

TEST_F(TabGroupsIPHControllerTest, DismissedOnMenuClosed) {
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(
                  Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1)
      .WillOnce(Return(true));

  for (int i = 0; i < 6; ++i)
    chrome::NewTab(browser());

  EXPECT_TRUE(promo_controller_->BubbleIsShowing(
      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature));
  iph_controller_->TabContextMenuOpened();
  EXPECT_FALSE(promo_controller_->BubbleIsShowing(
      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature));

  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1);

  iph_controller_->TabContextMenuClosed();
  EXPECT_FALSE(promo_controller_->BubbleIsShowing(
      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature));
}

TEST_F(TabGroupsIPHControllerTest, ShowsContextMenuHighlightIfAppropriate) {
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(
                  Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
      .Times(1);

  EXPECT_FALSE(iph_controller_->ShouldHighlightContextMenuItem());

  for (int i = 0; i < 6; ++i)
    chrome::NewTab(browser());

  EXPECT_TRUE(iph_controller_->ShouldHighlightContextMenuItem());
  iph_controller_->TabContextMenuOpened();
  iph_controller_->TabContextMenuClosed();
  EXPECT_FALSE(iph_controller_->ShouldHighlightContextMenuItem());
}
