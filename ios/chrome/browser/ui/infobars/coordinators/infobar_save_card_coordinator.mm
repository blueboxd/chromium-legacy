// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_save_card_coordinator.h"

#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarSaveCardCoordinator () <InfobarCoordinatorImplementation>

// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// ModalViewController owned by this Coordinator.
// TODO(crbug.com/1014652): Replace with CustomVC later on.
@property(nonatomic, strong) UIViewController* modalViewController;
// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly)
    autofill::AutofillSaveCardInfoBarDelegateMobile* saveCardInfoBarDelegate;

@end

@implementation InfobarSaveCardCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize modalViewController = _modalViewController;

- (instancetype)initWithInfoBarDelegate:
    (autofill::AutofillSaveCardInfoBarDelegateMobile*)saveCardInfoBarDelegate {
  self = [super initWithInfoBarDelegate:saveCardInfoBarDelegate
                           badgeSupport:YES
                                   type:InfobarType::kInfobarTypeSaveCard];
  if (self) {
    _saveCardInfoBarDelegate = saveCardInfoBarDelegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.started) {
    self.started = YES;
    // TODO(crbug.com/1014652): Configure and present Banner.
  }
}

- (void)stop {
  [super stop];
  if (self.started) {
    self.started = NO;
    // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
    // from memory.
    self.delegate->RemoveInfoBar();
    _saveCardInfoBarDelegate = nil;
    [self.infobarContainer childCoordinatorStopped:self];
  }
}

#pragma mark - InfobarCoordinatorImplementation

- (void)performInfobarAction {
  // TODO(crbug.com/1014652): Continue implementation.
}

- (void)infobarWasDismissed {
  // Release these strong ViewControllers at the time of infobar dismissal.
  self.bannerViewController = nil;
  self.modalViewController = nil;
  if (self.saveCardInfoBarDelegate)
    self.saveCardInfoBarDelegate->InfoBarDismissed();
}

#pragma mark Banner

- (void)infobarBannerWasPresented {
  // TODO(crbug.com/1014652): Continue implementation.
}

- (void)dismissBannerWhenInteractionIsFinished {
  // TODO(crbug.com/1014652): Continue implementation.
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  // TODO(crbug.com/1014652): Continue implementation.
}

#pragma mark Modal

- (BOOL)configureModalViewController {
  // TODO(crbug.com/1014652): Continue implementation.
  return NO;
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // TODO(crbug.com/1014652): Continue implementation.
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  // TODO(crbug.com/1014652): Continue implementation.
  return 0.0;
}

@end
