// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_confirm_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarConfirmCoordinator () <InfobarCoordinatorImplementation>

// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly) ConfirmInfoBarDelegate* confirmInfobarDelegate;
// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// InfobarModalViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarModalViewController* modalViewController;

@end

@implementation InfobarConfirmCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize modalViewController = _modalViewController;

- (instancetype)initWithInfoBarDelegate:
                    (ConfirmInfoBarDelegate*)confirmInfoBarDelegate
                                   type:(InfobarType)infobarType {
  self = [super initWithInfoBarDelegate:confirmInfoBarDelegate
                                   type:infobarType];
  if (self) {
    _confirmInfobarDelegate = confirmInfoBarDelegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.started) {
    self.started = YES;
    self.bannerViewController =
        [[InfobarBannerViewController alloc] initWithDelegate:self
                                                         type:self.infobarType];
    self.bannerViewController.titleText =
        base::SysUTF16ToNSString(self.confirmInfobarDelegate->GetMessageText());
    self.bannerViewController.buttonText =
        base::SysUTF16ToNSString(self.confirmInfobarDelegate->GetButtonLabel(
            ConfirmInfoBarDelegate::BUTTON_OK));
  }
}

- (void)stop {
  if (self.started) {
    self.started = NO;
    // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
    // from memory.
    self.delegate->RemoveInfoBar();
    _confirmInfobarDelegate = nil;
    [self.infobarContainer childCoordinatorStopped];
  }
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)configureModalViewController {
  self.modalViewController =
      [[InfobarModalViewController alloc] initWithModalDelegate:self];
  self.modalViewController.title =
      base::SysUTF16ToNSString(self.confirmInfobarDelegate->GetMessageText());
  return YES;
}

- (void)infobarBannerWasPresented {
  // NO-OP.
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // NO-OP.
}

- (void)dismissBannerWhenInteractionIsFinished {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (void)performInfobarAction {
  self.confirmInfobarDelegate->Accept();
}

- (void)infobarWasDismissed {
  // Release these strong ViewControllers at the time of infobar dismissal.
  self.bannerViewController = nil;
  self.modalViewController = nil;
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  // TODO(crbug.com/911864): Implement, this is a temporary value. If
  // InfobarConfirmCoordinator ends up having no Modal this should DCHECK or
  // NOTREACHED.
  return 50;
}

@end
