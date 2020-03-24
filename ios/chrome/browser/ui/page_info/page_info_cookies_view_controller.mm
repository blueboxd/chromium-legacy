// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_cookies_view_controller.h"

#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCookiesSwitch = kItemTypeEnumZero,
};

}  // namespace

@implementation PageInfoCookiesViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_COOKIES_TITLE);

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierContent];

  // Create the switch item.
  SettingsSwitchItem* cookiesSwitchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeCookiesSwitch];
  cookiesSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_COOKIES_SWITCH_LABEL);
  [self.tableViewModel addItem:cookiesSwitchItem
       toSectionWithIdentifier:SectionIdentifierContent];
  // TODO(crbug.com/1063824): Implement this.
}

@end
