// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"

#import "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSafetyCheckTableViewId = @"kSafetyCheckTableViewId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCheckTypes = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeUpdates = kItemTypeEnumZero,
  ItemTypePasswords,
  ItemTypeSafeBrowsing,
};

}  // namespace

@implementation SafetyCheckTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kSafetyCheckTableViewId;
  self.title =
      l10n_util::GetNSString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SAFETY_CHECK);

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Checks performed section.
  [model addSectionWithIdentifier:SectionIdentifierCheckTypes];
  [model addItem:[self updatesItem]
      toSectionWithIdentifier:SectionIdentifierCheckTypes];
  [model addItem:[self passwordsItem]
      toSectionWithIdentifier:SectionIdentifierCheckTypes];
  [model addItem:[self safeBrowsingItem]
      toSectionWithIdentifier:SectionIdentifierCheckTypes];
}

#pragma mark - Model Objects

- (TableViewItem*)updatesItem {
  return [self iconItemWithType:ItemTypeUpdates
                        titleID:IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_TITLE
                  iconImageName:nil];
}

- (TableViewItem*)passwordsItem {
  return [self iconItemWithType:ItemTypePasswords
                        titleID:IDS_IOS_SETTINGS_SAFETY_CHECK_PASSWORDS_TITLE
                  iconImageName:nil];
}

- (TableViewItem*)safeBrowsingItem {
  return
      [self iconItemWithType:ItemTypeSafeBrowsing
                     titleID:IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_TITLE
               iconImageName:nil];
}

#pragma mark - Item Constructor

// TODO(crbug.com/1078782): Change to custom item to handle safety check
// behavior.
// Construct a safety check item row with icon and title.
- (TableViewDetailIconItem*)iconItemWithType:(NSInteger)type
                                     titleID:(NSInteger)titleID
                               iconImageName:(NSString*)iconImageName {
  TableViewDetailIconItem* iconItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  iconItem.text = l10n_util::GetNSString(titleID);
  iconItem.iconImageName = iconImageName;

  return iconItem;
}

#pragma mark - UITableViewDelegate

// TODO(crbug.com/1078782): Add cases for updates, passwords, and safe browsing.
- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeUpdates:
      break;
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

@end
