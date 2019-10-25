// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/translate/translate_constants.h"
#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TranslateInfobarCoordinator () <TranslateInfobarDelegateObserving> {
  // Observer to listen for changes to the TranslateStep.
  std::unique_ptr<TranslateInfobarDelegateObserverBridge>
      _translateInfobarDelegateObserver;
}

// Delegate that holds the Translate Infobar information and actions.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* translateInfoBarDelegate;

// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;

// The current state of translate.
@property(nonatomic, assign) translate::TranslateStep currentStep;

// Tracks user actions taken throughout Translate lifetime.
@property(nonatomic, assign) UserAction userAction;

@end

@implementation TranslateInfobarCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate {
  self = [super initWithInfoBarDelegate:infoBarDelegate
                           badgeSupport:YES
                                   type:InfobarType::kInfobarTypeTranslate];
  if (self) {
    _translateInfoBarDelegate = infoBarDelegate;
    _translateInfobarDelegateObserver =
        std::make_unique<TranslateInfobarDelegateObserverBridge>(
            infoBarDelegate, self);
    _userAction = UserActionNone;
    _currentStep = translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;
  }
  return self;
}

#pragma mark - TranslateInfobarDelegateObserving

- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType {
  // TODO(crbug.com/1014959): implement
}

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate {
  return self.userAction == UserActionNone;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.started) {
    self.started = YES;
    self.bannerViewController = [[InfobarBannerViewController alloc]
        initWithDelegate:self
           presentsModal:self.hasBadge
                    type:InfobarType::kInfobarTypeTranslate];
    self.bannerViewController.titleText = [self titleText];
    self.bannerViewController.buttonText = [self buttonText];
    self.bannerViewController.iconImage =
        [UIImage imageNamed:@"infobar_translate_icon"];
    self.bannerViewController.optionalAccessibilityLabel =
        self.bannerViewController.titleText;
  }
}

- (void)stop {
  [super stop];
  if (self.started) {
    self.started = NO;
    // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
    // from memory.
    self.delegate->RemoveInfoBar();
    [self.infobarContainer childCoordinatorStopped:self];
  }
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)shouldBadgeBeAccepted {
  return self.currentStep ==
         translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE;
}

- (void)performInfobarAction {
  self.userAction |= UserActionTranslate;

  // TODO(crbug.com/1014959): Add metrics
  if (self.translateInfoBarDelegate->ShouldAutoAlwaysTranslate()) {
    self.translateInfoBarDelegate->ToggleAlwaysTranslate();
  }
  self.translateInfoBarDelegate->Translate();
}

- (void)infobarWasDismissed {
  // Release these strong ViewControllers at the time of infobar dismissal.
  self.bannerViewController = nil;
}

#pragma mark - Banner

- (void)infobarBannerWasPresented {
  // TODO(crbug.com/1014959): implement
}

- (void)dismissBannerWhenInteractionIsFinished {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  // TODO(crbug.com/1014959): implement
}

#pragma mark - Modal

- (BOOL)configureModalViewController {
  // TODO(crbug.com/1014959): implement
  return NO;
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // TODO(crbug.com/1014959): implement
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  // TODO(crbug.com/1014959): implement
  return 0.0;
}

#pragma mark - Private

- (NSString*)titleText {
  // TODO(crbug.com/1014959): Return different strings depending on
  // TranslateStep. Also, consider looking into base::i18n::MessageFormatter to
  // ensure the dstring can be translated properly with a dynamic variable
  // inserted.
  return [NSString
      stringWithFormat:@"%@ %@?", @"Translate",
                       base::SysUTF16ToNSString(self.translateInfoBarDelegate
                                                    ->target_language_name())];
}

- (NSString*)buttonText {
  // TODO(crbug.com/1014959): Return different strings depending on
  // TranslateStep.
  return l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_ACTION);
}

@end
