// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_save_card_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_table_view_controller.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarSaveCardCoordinator () <InfobarCoordinatorImplementation>

// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// InfobarSaveCardTableViewController owned by this Coordinator.
@property(nonatomic, strong)
    InfobarSaveCardTableViewController* modalViewController;
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
    self.bannerViewController = [[InfobarBannerViewController alloc]
        initWithDelegate:self
           presentsModal:self.hasBadge
                    type:InfobarType::kInfobarTypeSaveCard];
    self.bannerViewController.buttonText =
        base::SysUTF16ToNSString(self.saveCardInfoBarDelegate->GetButtonLabel(
            ConfirmInfoBarDelegate::BUTTON_OK));
    self.bannerViewController.titleText = base::SysUTF16ToNSString(
        self.saveCardInfoBarDelegate->GetMessageText());
    self.bannerViewController.subTitleText =
        base::SysUTF16ToNSString(self.saveCardInfoBarDelegate->card_label());
    gfx::Image icon = self.saveCardInfoBarDelegate->GetIcon();
    if (!icon.IsEmpty())
      self.bannerViewController.iconImage = icon.ToUIImage();
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

- (BOOL)isInfobarAccepted {
  return YES;
}

- (void)performInfobarAction {
  if (self.saveCardInfoBarDelegate->upload()) {
    // TODO(crbug.com/1014652): Open Modal if CreditCard details will be
    // uploaded. Meaning that the ToS needs to be displayed.
  } else if (self.saveCardInfoBarDelegate->Accept()) {
    // TODO(crbug.com/1014652): Until a post save editing functionality is
    // implemented the Infobar will be completely removed after its been
    // accepted.
    if (self.modalViewController) {
      [self dismissInfobarModal:self
                       animated:YES
                     completion:^{
                       // TODO(crbug.com/1014652): Once the badge is created
                       // confirm detachView completely removes it.
                       [self detachView];
                     }];
    } else {
      [self dismissInfobarBannerAnimated:YES
                              completion:^{
                                [self detachView];
                              }];
    }
  }
}

- (void)infobarWasDismissed {
  // Release these strong ViewControllers at the time of infobar dismissal.
  self.bannerViewController = nil;
  self.modalViewController = nil;
}

#pragma mark Banner

- (void)infobarBannerWasPresented {
  // TODO(crbug.com/1014652): Record metrics here if there's a distinction
  // between automatic and manual presentation.
}

- (void)dismissBannerWhenInteractionIsFinished {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  // TODO(crbug.com/1014652): Record metrics here if there's a distinction
  // between ignoring or dismissing the Infobar.
}

#pragma mark Modal

- (BOOL)configureModalViewController {
  // Return early if there's no delegate. e.g. A Modal presentation has been
  // triggered after the Infobar was destroyed, but before the badge/banner
  // were dismissed.
  if (!self.saveCardInfoBarDelegate)
    return NO;

  self.modalViewController =
      [[InfobarSaveCardTableViewController alloc] initWithModalDelegate:self];
  // TODO(crbug.com/1014652): Replace with Modal specific text.
  self.modalViewController.title =
      base::SysUTF16ToNSString(self.saveCardInfoBarDelegate->GetMessageText());

  return YES;
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // TODO(crbug.com/1014652): Check if there's a metric that should be recorded
  // here, or if there's a need to keep track of the presented state of the
  // Infobar for recording metrics on de-alloc. (See equivalent method on
  // InfobarPasswordCoordinator).
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  UITableView* tableView = self.modalViewController.tableView;
  // Update the tableView frame to then layout its content for |width|.
  tableView.frame = CGRectMake(0, 0, width, tableView.frame.size.height);
  [tableView setNeedsLayout];
  [tableView layoutIfNeeded];

  // Since the TableView is contained in a NavigationController get the
  // navigation bar height.
  CGFloat navigationBarHeight = self.modalViewController.navigationController
                                    .navigationBar.frame.size.height;

  return tableView.contentSize.height + navigationBarHeight;
}

@end
