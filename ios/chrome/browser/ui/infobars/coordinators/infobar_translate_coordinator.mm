// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_coordinator.h"

#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"

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

@end

@implementation TranslateInfobarCoordinator

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
  // TODO(crbug.com/1014959): implement
  return YES;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.started) {
    self.started = YES;
    // TODO(crbug.com/1014959): Configure BannerViewController.
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

- (void)performInfobarAction {
  // TODO(crbug.com/1014959): implement
}

- (void)infobarWasDismissed {
  // TODO(crbug.com/1014959): implement
}

#pragma mark - Banner

- (void)infobarBannerWasPresented {
  // TODO(crbug.com/1014959): implement
}

- (void)dismissBannerWhenInteractionIsFinished {
  // TODO(crbug.com/1014959): implement
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

@end
