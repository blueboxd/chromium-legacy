// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_groups_iph_controller.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/view.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Ref;
using ::testing::Return;

class TabGroupsIPHControllerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    mock_tracker_ =
        feature_engagement::TrackerFactory::GetInstance()
            ->SetTestingSubclassFactoryAndUse(
                GetProfile(), base::BindRepeating([](content::BrowserContext*) {
                  return std::make_unique<
                      feature_engagement::test::MockTracker>();
                }));

    // Other features call into the IPH backend. We don't want to fail
    // on their calls, so allow them. Our test cases will set
    // expectations for the calls they are interested in.
    EXPECT_CALL(*mock_tracker_, NotifyEvent(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    iph_controller_ = std::make_unique<TabGroupsIPHController>(
        browser(),
        base::BindRepeating(&TabGroupsIPHControllerTest::GetAnchorView,
                            base::Unretained(this)));
  }

  void TearDown() override {
    iph_controller_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  feature_engagement::test::MockTracker* mock_tracker_;
  std::unique_ptr<TabGroupsIPHController> iph_controller_;

 private:
  views::View* GetAnchorView(int tab_index) { return &dummy_anchor_view_; }

  // TabGroupsIPHController takes a callback to get the promo's anchor
  // view. These tests shouldn't trigger the promo, but we return this
  // view just in case.
  views::View dummy_anchor_view_;
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
