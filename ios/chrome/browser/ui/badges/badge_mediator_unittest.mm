// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_mediator.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_badge_model.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The two infobar types used in tests.  Both support badges.
InfobarType kFirstInfobarType = InfobarType::kInfobarTypePasswordSave;
InfobarType kSecondInfobarType = InfobarType::kInfobarTypePasswordUpdate;
}  // namespace

// Fake of BadgeConsumer.
@interface FakeBadgeConsumer : NSObject <BadgeConsumer>
@property(nonatomic, strong) id<BadgeItem> displayedBadge;
@property(nonatomic, assign) BOOL hasIncognitoBadge;
@property(nonatomic, assign) BOOL hasUnreadBadge;
@end

@implementation FakeBadgeConsumer
- (void)setupWithDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
                fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem {
  self.hasIncognitoBadge = fullscreenBadgeItem != nil;
  self.displayedBadge = displayedBadgeItem;
}
- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
             fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem {
  self.hasIncognitoBadge = fullscreenBadgeItem != nil;
  self.displayedBadge = displayedBadgeItem;
}
- (void)markDisplayedBadgeAsRead:(BOOL)read {
  self.hasUnreadBadge = !read;
}
@end

class BadgeMediatorTest : public PlatformTest {
 protected:
  BadgeMediatorTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        web_state_list_(
            std::make_unique<WebStateList>(&web_state_list_delegate_)) {
    feature_list_.InitAndEnableFeature(kInfobarUIReboot);
    badge_consumer_ = [[FakeBadgeConsumer alloc] init];
    badge_mediator_ =
        [[BadgeMediator alloc] initWithConsumer:badge_consumer_
                                   webStateList:web_state_list_.get()];
  }

  ~BadgeMediatorTest() override { [badge_mediator_ disconnect]; }

  // Adds a new WebState to the WebStateList and activates it.
  void AddAndActivateWebState(int index, bool incognito) {
    std::unique_ptr<web::TestWebState> web_state =
        std::make_unique<web::TestWebState>();
    web_state->SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(web_state.get());
    InfobarBadgeTabHelper::CreateForWebState(web_state.get());
    if (incognito) {
      web_state->SetBrowserState(
          browser_state_->GetOffTheRecordChromeBrowserState());
    } else {
      web_state->SetBrowserState(browser_state_.get());
    }
    web_state_list_->InsertWebState(index, std::move(web_state),
                                    WebStateList::INSERT_NO_FLAGS,
                                    WebStateOpener());
    web_state_list_->ActivateWebStateAt(index);
  }

  // Adds an Infobar of |type| to the InfoBarManager and returns the infobar.
  InfoBarIOS* AddInfobar(InfobarType type) {
    std::unique_ptr<InfoBarIOS> added_infobar =
        FakeInfobarIOS::Create(type, /*has_badge=*/true);
    InfoBarIOS* infobar = added_infobar.get();
    infobar_manager()->AddInfoBar(std::move(added_infobar));
    return infobar;
  }

  // Removes |infobar| from its manager.
  void RemoveInfobar(InfoBarIOS* infobar) {
    infobar_manager()->RemoveInfoBar(infobar);
  }

  // Returns the active WebState.
  web::WebState* web_state() { return web_state_list_->GetActiveWebState(); }
  // Returns the active WebState's InfoBarManagerImpl.
  InfoBarManagerImpl* infobar_manager() {
    return InfoBarManagerImpl::FromWebState(web_state());
  }
  // Returns the active WebState's InfobarBadgeTabHelper.
  InfobarBadgeTabHelper* tab_helper() {
    return InfobarBadgeTabHelper::FromWebState(web_state());
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment environment_;
  FakeBadgeConsumer* badge_consumer_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  BadgeMediator* badge_mediator_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
};

// Test that the BadgeMediator responds with no displayed and fullscreen badge
// when there are no Infobars added and the BrowserState is not OffTheRecord.
TEST_F(BadgeMediatorTest, BadgeMediatorTestNoInfobar) {
  AddAndActivateWebState(0, false);
  EXPECT_FALSE(badge_consumer_.displayedBadge);
  EXPECT_FALSE(badge_consumer_.hasIncognitoBadge);
}

// Test that the BadgeMediator responds with one new badge when an infobar is
// added
TEST_F(BadgeMediatorTest, BadgeMediatorTestAddInfobar) {
  AddAndActivateWebState(0, false);
  AddInfobar(kFirstInfobarType);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
}

// Test that the BadgeMediator handled the removal of the correct badge when two
// infobars are added and then one is removed.
TEST_F(BadgeMediatorTest, BadgeMediatorTestRemoveInfobar) {
  AddAndActivateWebState(0, false);
  AddInfobar(kFirstInfobarType);
  InfoBarIOS* second_infobar = AddInfobar(kSecondInfobarType);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypeOverflow);
  RemoveInfobar(second_infobar);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
}

TEST_F(BadgeMediatorTest, BadgeMediatorTestMarkAsRead) {
  AddAndActivateWebState(/*index=*/0, /*incognito=*/false);
  AddInfobar(kFirstInfobarType);
  // Since there is only one badge, it should be marked as read.
  EXPECT_FALSE(badge_consumer_.hasUnreadBadge);
  AddInfobar(kSecondInfobarType);
  ASSERT_EQ(BadgeType::kBadgeTypeOverflow,
            badge_consumer_.displayedBadge.badgeType);
  // Second badge should be unread since the overflow badge is being shown as
  // the displayed badge.
  EXPECT_TRUE(badge_consumer_.hasUnreadBadge);
  tab_helper()->UpdateBadgeForInfobarAccepted(kSecondInfobarType);
  // Second badge should be read since its infobar is accepted.
  EXPECT_FALSE(badge_consumer_.hasUnreadBadge);
}

// Test that the BadgeMediator updates the current badges to none when switching
// to a second WebState after an infobar is added to the first WebState.
TEST_F(BadgeMediatorTest, BadgeMediatorTestSwitchWebState) {
  AddAndActivateWebState(0, false);
  AddInfobar(kFirstInfobarType);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
  AddAndActivateWebState(1, false);
  EXPECT_FALSE(badge_consumer_.displayedBadge);
}

// Test that the BadgeMediator updates the badge when it is accepted.
TEST_F(BadgeMediatorTest, BadgeMediatorTestAcceptedBadge) {
  AddAndActivateWebState(0, false);
  AddInfobar(kFirstInfobarType);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_FALSE(badge_consumer_.displayedBadge.badgeState &= BadgeStateAccepted);

  tab_helper()->UpdateBadgeForInfobarAccepted(kFirstInfobarType);
  EXPECT_TRUE(badge_consumer_.displayedBadge.badgeState &= BadgeStateAccepted);
}

// Test that the BadgeMediator adds an incognito badge when the webstatelist
// changes.
TEST_F(BadgeMediatorTest, BadgeMediatorTestIncognito) {
  AddAndActivateWebState(0, true);
  EXPECT_TRUE(badge_consumer_.hasIncognitoBadge);
}

// Test that the BadgeMediator updates the current badges when the starting
// active WebState already has a badge. This simulates an app launch after an
// update when the WebStateList is preserved but the LocationBar (and therefore
// the BadgeMediator) is restarted from scratch.
TEST_F(BadgeMediatorTest, BadgeMediatorTestRestartWithInfobar) {
  AddAndActivateWebState(0, false);
  AddInfobar(kFirstInfobarType);

  // Simulate reload of app, but preservation of WebStateList.
  badge_mediator_ = nil;
  badge_consumer_ = nil;

  badge_consumer_ = [[FakeBadgeConsumer alloc] init];
  badge_mediator_ =
      [[BadgeMediator alloc] initWithConsumer:badge_consumer_
                                 webStateList:web_state_list_.get()];
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
}

// Test that the BadgeMediator clears its badges when the last WebState is
// detached and a new WebState is added. This test also makes sure that closing
// the last WebState doesn't break anything.
TEST_F(BadgeMediatorTest, BadgeMediatorTestCloseLastTab) {
  AddAndActivateWebState(0, false);
  AddInfobar(kFirstInfobarType);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
  web_state_list_->DetachWebStateAt(0);
  AddAndActivateWebState(0, false);
  ASSERT_FALSE(badge_consumer_.displayedBadge);
}
