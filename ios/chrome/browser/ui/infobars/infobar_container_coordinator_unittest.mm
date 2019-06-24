// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_coordinator.h"

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/confirm_infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_confirm_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Exposed for testing.
@interface InfobarContainerCoordinator (Testing)
@property(nonatomic, assign) BOOL legacyContainerFullscrenSupportDisabled;
@end

// Test ContainerCoordinatorPositioner.
@interface TestContainerCoordinatorPositioner : NSObject <InfobarPositioner>
@property(nonatomic, strong) UIView* baseView;
@end
@implementation TestContainerCoordinatorPositioner
- (UIView*)parentView {
  return self.baseView;
}
@end

// Test fixture for testing InfobarContainerCoordinatorTest.
class InfobarContainerCoordinatorTest : public PlatformTest {
 protected:
  InfobarContainerCoordinatorTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        web_state_list_(
            std::make_unique<WebStateList>(&web_state_list_delegate_)),
        base_view_controller_([[UIViewController alloc] init]),
        positioner_([[TestContainerCoordinatorPositioner alloc] init]) {
    // Enable kInfobarUIReboot flag.
    feature_list_.InitAndEnableFeature(kInfobarUIReboot);

    // Setup WebstateList, Webstate and NavigationManager (Needed for
    // InfobarManager).
    std::unique_ptr<web::TestWebState> web_state =
        std::make_unique<web::TestWebState>();
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager->SetBrowserState(browser_state_.get());
    navigation_manager_ = navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_state_.get());
    web_state_list_->InsertWebState(0, std::move(web_state),
                                    WebStateList::INSERT_NO_FLAGS,
                                    WebStateOpener());
    web_state_list_->ActivateWebStateAt(0);
    std::unique_ptr<web::TestWebState> second_web_state =
        std::make_unique<web::TestWebState>();
    web_state_list_->InsertWebState(1, std::move(second_web_state),
                                    WebStateList::INSERT_NO_FLAGS,
                                    WebStateOpener());

    // Setup InfobarBadgeTabHelper and InfoBarManager
    InfobarBadgeTabHelper::CreateForWebState(
        web_state_list_->GetActiveWebState());
    InfobarBadgeTabHelper::CreateForWebState(web_state_list_->GetWebStateAt(1));
    InfoBarManagerImpl::CreateForWebState(web_state_list_->GetActiveWebState());

    // Setup the InfobarContainerCoordinator.
    infobar_container_coordinator_ = [[InfobarContainerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                      browserState:browser_state_.get()
                      webStateList:web_state_list_.get()];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    positioner_.baseView = base_view_controller_.view;
    infobar_container_coordinator_.positioner = positioner_;
    infobar_container_coordinator_.commandDispatcher =
        OCMClassMock([CommandDispatcher class]);
    infobar_container_coordinator_.legacyContainerFullscrenSupportDisabled =
        YES;
    [infobar_container_coordinator_ start];

    // Setup the InfobarCoordinator and InfobarDelegate.
    TestInfoBarDelegate* test_infobar_delegate =
        new TestInfoBarDelegate(@"Title");
    coordinator_ = [[InfobarConfirmCoordinator alloc]
        initWithInfoBarDelegate:test_infobar_delegate
                           type:InfobarType::kInfobarTypeConfirm];
    infobar_delegate_ =
        std::unique_ptr<ConfirmInfoBarDelegate>(test_infobar_delegate);

    // Setup the Legacy InfobarController and InfobarDelegate.
    TestInfoBarDelegate* test_legacy_infobar_delegate =
        new TestInfoBarDelegate(@"Legacy Infobar");
    legacy_controller_ = [[ConfirmInfoBarController alloc]
        initWithInfoBarDelegate:test_legacy_infobar_delegate];
    legacy_infobar_delegate_ =
        std::unique_ptr<ConfirmInfoBarDelegate>(test_legacy_infobar_delegate);
  }

  ~InfobarContainerCoordinatorTest() override {
    // Make sure InfobarBanner has been dismissed.
    if (infobar_container_coordinator_.infobarBannerState ==
        InfobarBannerPresentationState::Presented) {
      [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                        completion:nil];
      EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^bool {
            return infobar_container_coordinator_.infobarBannerState ==
                   InfobarBannerPresentationState::NotPresented;
          }));
    }
    [infobar_container_coordinator_ stop];
  }

  // Adds an Infobar to the InfobarManager, triggering an InfobarBanner
  // presentation.
  void AddInfobar() {
    GetInfobarManager()->AddInfoBar(std::make_unique<InfoBarIOS>(
        coordinator_, std::move(infobar_delegate_)));
  }

  // Adds a Legacy Infobar to the InfobarManager, triggering an InfobarBanner
  // presentation.
  void AddLegacyInfobar() {
    GetInfobarManager()->AddInfoBar(std::make_unique<InfoBarIOS>(
        legacy_controller_, std::move(legacy_infobar_delegate_)));
  }

  // Returns InfoBarManager attached to web_state_.
  infobars::InfoBarManager* GetInfobarManager() {
    return InfoBarManagerImpl::FromWebState(
        web_state_list_->GetActiveWebState());
  }

  base::test::ScopedTaskEnvironment environment_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  InfobarContainerCoordinator* infobar_container_coordinator_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  web::TestNavigationManager* navigation_manager_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  TestContainerCoordinatorPositioner* positioner_;
  InfobarConfirmCoordinator* coordinator_;
  std::unique_ptr<ConfirmInfoBarDelegate> infobar_delegate_;
  ConfirmInfoBarController* legacy_controller_;
  std::unique_ptr<ConfirmInfoBarDelegate> legacy_infobar_delegate_;
};

// Tests infobarBannerState is InfobarBannerPresentationState::Presented once an
// InfobarBanner is presented.
TEST_F(InfobarContainerCoordinatorTest,
       InfobarBannerPresentationStatePresented) {
  EXPECT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
  AddInfobar();
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is automatically dismissed after
// kInfobarBannerPresentationDurationInSeconds seconds.
TEST_F(InfobarContainerCoordinatorTest, TestAutomaticInfobarBannerDismissal) {
  EXPECT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);

  AddInfobar();

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));

  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kInfobarBannerPresentationDurationInSeconds, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is correctly dismissed after calling
// dismissInfobarBannerAnimated.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarBannerDismissal) {
  EXPECT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);

  AddInfobar();

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
}

// Tests that a legacy Infobar can be presented and
// infobarBannerState is still NotPresented.
TEST_F(InfobarContainerCoordinatorTest, TestLegacyInfobarPresentation) {
  EXPECT_FALSE([infobar_container_coordinator_
      isInfobarPresentingForWebState:web_state_list_->GetActiveWebState()]);
  EXPECT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::NotPresented);
  AddLegacyInfobar();
  EXPECT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
  EXPECT_TRUE([infobar_container_coordinator_
      isInfobarPresentingForWebState:web_state_list_->GetActiveWebState()]);
}

// Tests that the presentation of a LegacyInfobar doesn't dismiss the previously
// presented InfobarBanner.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerPresentationBeforeLegacyPresentation) {
  EXPECT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
  AddInfobar();
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);
  AddLegacyInfobar();
  EXPECT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);
}

// Tests that a presented LegacyInfobar doesn't interfere with presenting an
// InfobarBanner.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerPresentationAfterLegacyPresentation) {
  EXPECT_FALSE([infobar_container_coordinator_
      isInfobarPresentingForWebState:web_state_list_->GetActiveWebState()]);
  AddLegacyInfobar();
  ASSERT_TRUE([infobar_container_coordinator_
      isInfobarPresentingForWebState:web_state_list_->GetActiveWebState()]);
  ASSERT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
  AddInfobar();
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is dismissed when changing Webstates.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerDismissAtWebStateChange) {
  AddInfobar();

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);

  web_state_list_->ActivateWebStateAt(1);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is not presented again after returning from a
// different Webstate.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerNotPresentAfterWebStateChange) {
  AddInfobar();

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);

  web_state_list_->ActivateWebStateAt(1);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !(infobar_container_coordinator_.infobarBannerState ==
                 InfobarBannerPresentationState::Presented);
      }));
  ASSERT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);

  web_state_list_->ActivateWebStateAt(0);
  // Wait for any potential presentation.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(1));

  ASSERT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
}

// Tests infobarBannerState is NotPresented once an InfobarBanner has been
// dismissed directly by its base VC.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarBannerDismissalByBaseVC) {
  AddInfobar();
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);

  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::NotPresented);
}

// Tests that the ChildCoordinator is deleted once it stops.
// TODO(crbug.com/961343): Update test when more than one Child Coordinator is
// supported.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarChildCoordinatorCount) {
  AddInfobar();

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);

  ASSERT_EQ(NSUInteger(1),
            infobar_container_coordinator_.childCoordinators.count);
  ASSERT_TRUE(infobar_container_coordinator_.infobarBannerState ==
              InfobarBannerPresentationState::Presented);

  // Dismiss the Banner before closing the Webstate to avoid bug 966057.
  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  web_state_list_->CloseWebStateAt(0, 0);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);
  ASSERT_EQ(NSUInteger(0),
            infobar_container_coordinator_.childCoordinators.count);
}
