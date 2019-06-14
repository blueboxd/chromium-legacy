// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Banner View constants.
const CGFloat kiPhoneBannerOverlapWithOmnibox = 5.0;
const CGFloat kiPadBannerOverlapWithOmnibox = 10.0;
}  // namespace

@interface InfobarCoordinator () <InfobarCoordinatorImplementation,
                                  InfobarBannerPositioner,
                                  InfobarModalPositioner> {
  // The AnimatedFullscreenDisable disables fullscreen by displaying the
  // Toolbar/s when an Infobar banner is presented.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> animatedFullscreenDisabler_;
}

// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly) infobars::InfoBarDelegate* infobarDelegate;
// The transition delegate used by the Coordinator to present the InfobarBanner.
// nil if no Banner is being presented.
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;
// The transition delegate used by the Coordinator to present the InfobarModal.
// nil if no Modal is being presented.
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;
// Readwrite redefinition.
@property(nonatomic, assign, readwrite) BOOL bannerWasPresented;

@end

@implementation InfobarCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize baseViewController = _baseViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize browserState = _browserState;
// Property defined in InfobarUIDelegate.
@synthesize delegate = _delegate;
// Property defined in InfobarUIDelegate.
@synthesize infobarType = _infobarType;
// Property defined in InfobarUIDelegate.
@synthesize presented = _presented;

- (instancetype)initWithInfoBarDelegate:
                    (infobars::InfoBarDelegate*)infoBarDelegate
                                   type:(InfobarType)infobarType {
  self = [super initWithBaseViewController:nil browserState:nil];
  if (self) {
    _infobarDelegate = infoBarDelegate;
    _presented = YES;
    _infobarType = infobarType;
  }
  return self;
}

#pragma mark - Public Methods.

- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion {
  DCHECK(self.browserState);
  DCHECK(self.baseViewController);
  DCHECK(self.bannerViewController);

  // Make sure to display the Toolbar/s before presenting the Banner.
  animatedFullscreenDisabler_ =
      std::make_unique<AnimatedScopedFullscreenDisabler>(
          FullscreenControllerFactory::GetInstance()->GetForBrowserState(
              self.browserState));
  animatedFullscreenDisabler_->StartAnimation();

  [self.bannerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerTransitionDriver.bannerPositioner = self;
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController
      presentViewController:self.bannerViewController
                   animated:animated
                 completion:^{
                   [weakSelf
                       configureAccessibilityForBannerInViewController:
                           weakSelf.baseViewController
                                                            presenting:YES];
                   weakSelf.presentingInfobarBanner = YES;
                   weakSelf.bannerWasPresented = YES;
                   [weakSelf infobarBannerWasPresented];
                   if (completion)
                     completion();
                 }];
}

- (void)presentInfobarModal {
  ProceduralBlock modalPresentation = ^{
    DCHECK(!self.bannerViewController);
    DCHECK(self.baseViewController);
    self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
        initWithTransitionMode:InfobarModalTransitionBase];
    self.modalTransitionDriver.modalPositioner = self;
    [self presentInfobarModalFrom:self.baseViewController
                           driver:self.modalTransitionDriver];
    [self infobarModalPresentedFromBanner:NO];
  };

  // Dismiss InfobarBanner first if being presented.
  if (self.baseViewController.presentedViewController) {
    [self dismissInfobarBanner:self animated:NO completion:modalPresentation];
  } else {
    modalPresentation();
  }
}

- (void)dismissInfobarBannerAfterInteraction {
  if (!self.modalTransitionDriver) {
    [self dismissBannerWhenInteractionIsFinished];
  }
}

- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion {
  [self dismissInfobarBanner:self animated:animated completion:completion];
}

#pragma mark - Protocols

#pragma mark InfobarUIDelegate

- (void)removeView {
  [self dismissInfobarBanner:self animated:YES completion:nil];
}

- (void)detachView {
  [self dismissInfobarBanner:self animated:YES completion:nil];
  [self stop];
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  [self performInfobarAction];
  [self.badgeDelegate infobarWasAccepted];
  [self dismissInfobarBanner:sender animated:YES completion:nil];
}

- (void)presentInfobarModalFromBanner {
  DCHECK(self.bannerViewController);
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  self.modalTransitionDriver.modalPositioner = self;
  [self presentInfobarModalFrom:self.bannerViewController
                         driver:self.modalTransitionDriver];
  [self infobarModalPresentedFromBanner:YES];
}

- (void)dismissInfobarBanner:(id)sender
                    animated:(BOOL)animated
                  completion:(void (^)())completion {
  DCHECK(self.baseViewController);
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             if (completion)
                               completion();
                           }];
  } else {
    if (completion)
      completion();
  }
}

- (void)infobarBannerWasDismissed {
  self.presentingInfobarBanner = NO;
  [self configureAccessibilityForBannerInViewController:self.baseViewController
                                             presenting:NO];
  [self.badgeDelegate infobarBannerWasDismissed];
  self.bannerTransitionDriver = nil;
  animatedFullscreenDisabler_ = nullptr;
  [self infobarWasDismissed];
}

#pragma mark InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  NamedGuide* omniboxGuide =
      [NamedGuide guideWithName:kOmniboxGuide
                           view:self.baseViewController.view];
  UIView* omniboxView = omniboxGuide.constrainedView;
  CGRect omniboxFrame = omniboxView.frame;

  // TODO(crbug.com/964136): The TabStrip on iPad is pushing down the Omnibox
  // view. On iPad when the TabStrip is visible convert the Omnibox frame to the
  // self.baseViewController.view coordinate system.
  CGFloat bannerOverlap = kiPhoneBannerOverlapWithOmnibox;
  NamedGuide* tabStripGuide =
      [NamedGuide guideWithName:kTabStripTabSwitcherGuide
                           view:self.baseViewController.view];
  if (tabStripGuide.constrainedFrame.size.height) {
    omniboxFrame = [omniboxView convertRect:omniboxView.frame
                                     toView:self.baseViewController.view];
    bannerOverlap = kiPadBannerOverlapWithOmnibox;
  }

  return omniboxFrame.origin.y + omniboxFrame.size.height - bannerOverlap;
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}

#pragma mark InfobarModalDelegate

- (void)modalInfobarButtonWasAccepted:(id)sender {
  [self performInfobarAction];
  [self.badgeDelegate infobarWasAccepted];
  [self dismissInfobarModal:sender animated:YES completion:nil];
}

- (void)dismissInfobarModal:(id)sender
                   animated:(BOOL)animated
                 completion:(ProceduralBlock)completion {
  DCHECK(self.baseViewController);
  if (self.baseViewController.presentedViewController) {
    // Deselect infobar badge in parallel with modal dismissal.
    [self.badgeDelegate infobarModalWillDismiss];
    __weak __typeof(self) weakSelf = self;

    // If the Modal is being presented by the Banner, call dismiss on it.
    // This way the modal dismissal will animate correctly and the completion
    // block cleans up the banner correctly.
    if (self.bannerViewController.presentedViewController) {
      [self.bannerViewController
          dismissViewControllerAnimated:animated
                             completion:^{
                               [weakSelf
                                   dismissInfobarBannerAnimated:NO
                                                     completion:completion];
                             }];
    } else {
      [self.baseViewController dismissViewControllerAnimated:animated
                                                  completion:^{
                                                    if (completion)
                                                      completion();
                                                  }];
    }
  } else {
    if (completion)
      completion();
  }
}

- (void)modalInfobarWasDismissed:(id)sender {
  // infobarModalWillDismiss call is needed, because sometimes the
  // baseViewController will dismiss the modal without going through the
  // coordinator.
  [self.badgeDelegate infobarModalWillDismiss];
  self.modalTransitionDriver = nil;

  // If InfobarBanner is being presented it means that this Modal was presented
  // by an InfobarBanner. If this is the case InfobarBanner will call
  // infobarWasDismissed and clean up once it gets dismissed, this prevents
  // counting the dismissal metrics twice.
  if (!self.presentingInfobarBanner)
    [self infobarWasDismissed];
}

#pragma mark InfobarModalPositioner

- (CGFloat)modalHeight {
  return [self infobarModalHeight];
}

#pragma mark InfobarCoordinatorImplementation

- (void)configureModalViewController {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarBannerWasPresented {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  NOTREACHED() << "Subclass must implement.";
}

- (void)dismissBannerWhenInteractionIsFinished {
  NOTREACHED() << "Subclass must implement.";
}

- (void)performInfobarAction {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarWasDismissed {
  NOTREACHED() << "Subclass must implement.";
}

- (CGFloat)infobarModalHeight {
  NOTREACHED() << "Subclass must implement.";
  return 0;
}

#pragma mark - Private

- (void)presentInfobarModalFrom:(UIViewController*)presentingViewController
                         driver:(InfobarModalTransitionDriver*)driver {
  [self configureModalViewController];
  DCHECK(self.modalViewController);
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = driver;
  navController.modalPresentationStyle = UIModalPresentationCustom;
  [presentingViewController presentViewController:navController
                                         animated:YES
                                       completion:nil];
  [self.badgeDelegate infobarModalWasPresented];
}

// Configures the Banner Accessibility in order to give VoiceOver users the
// ability to select other elements while the banner is presented. Call this
// method after the Banner has been presented or dismissed. |presenting| is YES
// if banner was presented, NO if dismissed.
- (void)configureAccessibilityForBannerInViewController:
            (UIViewController*)presentingViewController
                                             presenting:(BOOL)presenting {
  if (presenting) {
    // Set the banner's superview accessibilityViewIsModal property to NO. This
    // will allow the selection of the banner sibling views e.g. the
    // presentingViewController views.
    self.bannerViewController.view.superview.accessibilityViewIsModal = NO;

    // Make sure the banner is an accessibility element of the
    // PresentingViewController.
    presentingViewController.accessibilityElements =
        @[ self.bannerViewController.view, presentingViewController.view ];

    // Finally, focus the banner.
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    self.bannerViewController.view);
  } else {
    // Remove the Banner as an A11y element.
    presentingViewController.accessibilityElements =
        @[ presentingViewController.view ];
  }
}

@end
