// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_presentation_state.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
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
// NavigationController that contains the modalViewController.
@property(nonatomic, weak) UINavigationController* modalNavigationController;
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
@synthesize hasBadge = _hasBadge;
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
    _hasBadge = YES;
    _infobarType = infobarType;
  }
  return self;
}

#pragma mark - Public Methods.

- (void)stop {
  animatedFullscreenDisabler_ = nullptr;
  _infobarDelegate = nil;
}

- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion {
  DCHECK(self.browserState);
  DCHECK(self.baseViewController);
  DCHECK(self.bannerViewController);

  // If |self.baseViewController| is not part of the ViewHierarchy the banner
  // shouldn't be presented.
  if (!self.baseViewController.view.window) {
    return;
  }

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
  if ([self.bannerViewController
          conformsToProtocol:@protocol(InfobarBannerInteractable)]) {
    UIViewController<InfobarBannerInteractable>* interactableBanner =
        base::mac::ObjCCastStrict<UIViewController<InfobarBannerInteractable>>(
            self.bannerViewController);
    interactableBanner.interactionDelegate = self.bannerTransitionDriver;
  }

  self.infobarBannerState = InfobarBannerPresentationState::IsAnimating;
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController
      presentViewController:self.bannerViewController
                   animated:animated
                 completion:^{
                   [weakSelf
                       configureAccessibilityForBannerInViewController:
                           weakSelf.baseViewController
                                                            presenting:YES];
                   weakSelf.bannerWasPresented = YES;
                   weakSelf.infobarBannerState =
                       InfobarBannerPresentationState::Presented;
                   [weakSelf infobarBannerWasPresented];
                   if (completion)
                     completion();
                 }];
}

- (void)presentInfobarModal {
  ProceduralBlock modalPresentation = ^{
    DCHECK(self.infobarBannerState !=
           InfobarBannerPresentationState::Presented);
    DCHECK(self.baseViewController);
    self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
        initWithTransitionMode:InfobarModalTransitionBase];
    self.modalTransitionDriver.modalPositioner = self;
    __weak __typeof(self) weakSelf = self;
    [self presentInfobarModalFrom:self.baseViewController
                           driver:self.modalTransitionDriver
                       completion:^{
                         [weakSelf infobarModalPresentedFromBanner:NO];
                       }];
  };

  // Dismiss InfobarBanner first if being presented.
  if (self.baseViewController.presentedViewController &&
      self.baseViewController.presentedViewController ==
          self.bannerViewController) {
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
  [self dismissInfobarBanner:self animated:NO completion:nil];
  [self dismissInfobarModal:self animated:NO completion:nil];
  [self stop];
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  [self performInfobarAction];
  [self.badgeDelegate infobarWasAccepted:self.infobarType
                             forWebState:self.webState];
  [self dismissInfobarBanner:sender animated:YES completion:nil];
}

- (void)presentInfobarModalFromBanner {
  DCHECK(self.bannerViewController);
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  self.modalTransitionDriver.modalPositioner = self;
  __weak __typeof(self) weakSelf = self;
  [self presentInfobarModalFrom:self.bannerViewController
                         driver:self.modalTransitionDriver
                     completion:^{
                       [weakSelf infobarModalPresentedFromBanner:YES];
                     }];
}

- (void)dismissInfobarBanner:(id)sender
                    animated:(BOOL)animated
                  completion:(void (^)())completion
               userInitiated:(BOOL)userInitiated {
  DCHECK(self.baseViewController);
  // Make sure the banner is completely presented before trying to dismiss it.
  [self.bannerTransitionDriver completePresentationTransitionIfRunning];

  if (self.baseViewController.presentedViewController &&
      self.baseViewController.presentedViewController ==
          self.bannerViewController) {
    [self infobarBannerWillBeDismissed:userInitiated];
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
  self.infobarBannerState = InfobarBannerPresentationState::NotPresented;
  [self configureAccessibilityForBannerInViewController:self.baseViewController
                                             presenting:NO];
  self.bannerTransitionDriver = nil;
  animatedFullscreenDisabler_ = nullptr;
  [self infobarWasDismissed];
  [self.infobarContainer childCoordinatorBannerWasDismissed:self.infobarType];
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
  [self.badgeDelegate infobarWasAccepted:self.infobarType
                             forWebState:self.webState];
  [self dismissInfobarModal:sender animated:YES completion:nil];
}

- (void)dismissInfobarModal:(id)sender
                   animated:(BOOL)animated
                 completion:(ProceduralBlock)completion {
  DCHECK(self.baseViewController);
  if (self.baseViewController.presentedViewController) {
    // If the Modal is being presented by the Banner, call dismiss on it.
    // This way the modal dismissal will animate correctly and the completion
    // block cleans up the banner correctly.
    if (self.baseViewController.presentedViewController ==
        self.bannerViewController) {
      __weak __typeof(self) weakSelf = self;
      [self.bannerViewController
          dismissViewControllerAnimated:animated
                             completion:^{
                               [weakSelf
                                   dismissInfobarBannerAnimated:NO
                                                     completion:completion];
                             }];

    } else if (self.baseViewController.presentedViewController ==
               self.modalNavigationController) {
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
  self.modalTransitionDriver = nil;

  // If InfobarBanner is being presented it means that this Modal was presented
  // by an InfobarBanner. If this is the case InfobarBanner will call
  // infobarWasDismissed and clean up once it gets dismissed, this prevents
  // counting the dismissal metrics twice.
  if (self.infobarBannerState != InfobarBannerPresentationState::Presented)
    [self infobarWasDismissed];
}

#pragma mark InfobarModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  return [self infobarModalHeightForWidth:width];
}

#pragma mark InfobarCoordinatorImplementation

- (BOOL)configureModalViewController {
  NOTREACHED() << "Subclass must implement.";
  return NO;
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

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarWasDismissed {
  NOTREACHED() << "Subclass must implement.";
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  NOTREACHED() << "Subclass must implement.";
  return 0;
}

#pragma mark - Private

// |presentingViewController| presents the InfobarModal using |driver|. If
// Modal is presented successfully |completion| will be executed.
- (void)presentInfobarModalFrom:(UIViewController*)presentingViewController
                         driver:(InfobarModalTransitionDriver*)driver
                     completion:(ProceduralBlock)completion {
  BOOL infobarWasConfigured = [self configureModalViewController];
  if (!infobarWasConfigured) {
    if (driver.transitionMode == InfobarModalTransitionBanner) {
      [self dismissInfobarBannerAnimated:NO completion:nil];
    }
    return;
  }

  DCHECK(self.modalViewController);
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = driver;
  navController.modalPresentationStyle = UIModalPresentationCustom;
  self.modalNavigationController = navController;
  [presentingViewController presentViewController:navController
                                         animated:YES
                                       completion:completion];
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

// Helper method for non-user initiated InfobarBanner dismissals.
- (void)dismissInfobarBanner:(id)sender
                    animated:(BOOL)animated
                  completion:(void (^)())completion {
  [self dismissInfobarBanner:sender
                    animated:animated
                  completion:completion
               userInitiated:NO];
}

@end
