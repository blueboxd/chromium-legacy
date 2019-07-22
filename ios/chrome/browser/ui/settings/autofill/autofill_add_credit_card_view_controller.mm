// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"

#include "base/feature_list.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AutofillAddCreditCardViewController

- (instancetype)init {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  return [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsSelection = NO;
  self.view.backgroundColor = UIColor.cr_systemGroupedBackgroundColor;

  self.navigationItem.title = l10n_util::GetNSString(
      IDS_IOS_CREDIT_CARD_SETTINGS_ADD_PAYMENT_METHOD_TITLE);

  // Adds 'Cancel' and 'Add' buttons to Navigation bar.
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(handleCancelButton:)];
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_ADD_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:nil];
}

#pragma mark - Private

// Dimisses this view controller.
- (void)handleCancelButton:(id)sender {
  [self.navigationController dismissViewControllerAnimated:YES completion:nil];
}

@end
