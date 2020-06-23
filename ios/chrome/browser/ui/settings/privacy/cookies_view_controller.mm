// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/cookies_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_commands.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAllowCookies = kItemTypeEnumZero,
  ItemTypeBlockThirdPartyCookiesIncognito,
  ItemTypeBlockThirdPartyCookies,
  ItemTypeBlockAllCookies,
  ItemTypeCookiesDescriptionFooter,
};

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCookiesContent = kSectionIdentifierEnumZero,
};

}  // namespace

@interface PrivacyCookiesViewController ()

@property(nonatomic, strong) TableViewItem* selectedCookiesItem;
@property(nonatomic, assign) ItemType selectedSetting;

@end

@implementation PrivacyCookiesViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_COOKIES);
  self.styler.cellBackgroundColor = UIColor.cr_systemBackgroundColor;
  self.styler.tableViewBackgroundColor = UIColor.cr_systemBackgroundColor;
  self.tableView.backgroundColor = self.styler.tableViewBackgroundColor;

  if (!base::FeatureList::IsEnabled(kSettingsRefresh))
    [self.tableView setSeparatorStyle:UITableViewCellSeparatorStyleNone];

  if (!self.tableViewModel)
    [self loadModel];

  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:self.selectedSetting];
  [self updateSelectedCookiesItemWithIndexPath:indexPath];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate privacyCookiesViewControllerWasRemoved:self];
  }
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewInfoButtonItem* allowCookies =
      [[TableViewInfoButtonItem alloc] initWithType:ItemTypeAllowCookies];
  allowCookies.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_ALLOW_COOKIES_TITLE);
  allowCookies.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_ALLOW_COOKIES_DETAIL);
  allowCookies.useCustomSeparator = YES;
  allowCookies.iconImageName = @"accessory_no_checkmark";
  [self.tableViewModel addItem:allowCookies
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewInfoButtonItem* blockThirdPartyCookiesIncognito =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeBlockThirdPartyCookiesIncognito];
  blockThirdPartyCookiesIncognito.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_INCOGNITO_TITLE);
  blockThirdPartyCookiesIncognito.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_DETAIL);
  blockThirdPartyCookiesIncognito.useCustomSeparator = YES;
  blockThirdPartyCookiesIncognito.iconImageName = @"accessory_no_checkmark";
  [self.tableViewModel addItem:blockThirdPartyCookiesIncognito
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewInfoButtonItem* blockThirdPartyCookies =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeBlockThirdPartyCookies];
  blockThirdPartyCookies.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_TITLE);
  blockThirdPartyCookies.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_DETAIL);
  blockThirdPartyCookies.useCustomSeparator = YES;
  blockThirdPartyCookies.iconImageName = @"accessory_no_checkmark";
  [self.tableViewModel addItem:blockThirdPartyCookies
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewInfoButtonItem* blockAllCookies =
      [[TableViewInfoButtonItem alloc] initWithType:ItemTypeBlockAllCookies];
  blockAllCookies.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_ALL_COOKIES_TITLE);
  blockAllCookies.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_ALL_COOKIES_DETAIL);
  blockAllCookies.useCustomSeparator = YES;
  blockAllCookies.iconImageName = @"accessory_no_checkmark";
  [self.tableViewModel addItem:blockAllCookies
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewTextLinkItem* cookiesDescriptionFooter =
      [[TableViewTextLinkItem alloc]
          initWithType:ItemTypeCookiesDescriptionFooter];
  cookiesDescriptionFooter.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_COOKIES_FOOTER);
  [self.tableViewModel addItem:cookiesDescriptionFooter
       toSectionWithIdentifier:SectionIdentifierCookiesContent];
}

#pragma mark - Private

// Adds Checkmark icon to the selected item.
// Removes the checkmark icon of the prevous selected option item.
- (void)updateSelectedCookiesItemWithIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/1095579): Tint this.
  TableViewItem* previousSelectedCookiesItem;
  if (self.selectedCookiesItem) {
    previousSelectedCookiesItem = self.selectedCookiesItem;
    static_cast<TableViewInfoButtonItem*>(previousSelectedCookiesItem)
        .iconImageName = @"accessory_no_checkmark";
  }
  self.selectedCookiesItem = [self.tableViewModel itemAtIndexPath:indexPath];
  TableViewInfoButtonItem* selectedItem =
      static_cast<TableViewInfoButtonItem*>(self.selectedCookiesItem);
  selectedItem.iconImageName = @"accessory_checkmark";
  selectedItem.tintColor = [UIColor colorNamed:kBlueColor];

  if (previousSelectedCookiesItem)
    [self reconfigureCellsForItems:@[
      previousSelectedCookiesItem, self.selectedCookiesItem
    ]];
}

// Returns the ItemType associated with the CookiesSettingType.
- (ItemType)itemTypeForCookiesSettingType:(CookiesSettingType)settingType {
  switch (settingType) {
    case SettingTypeBlockThirdPartyCookiesIncognito:
      return ItemTypeBlockThirdPartyCookiesIncognito;
    case SettingTypeBlockThirdPartyCookies:
      return ItemTypeBlockThirdPartyCookies;
    case SettingTypeBlockAllCookies:
      return ItemTypeBlockAllCookies;
    case SettingTypeAllowCookies:
      return ItemTypeAllowCookies;
  }
}

#pragma mark - Actions

// Called when the user clicks on the information button.
- (void)didTapInfoIcon:(UIButton*)button {
  NSString* message;
  switch (button.tag) {
    case ItemTypeAllowCookies:
      message = l10n_util::GetNSString(
          IDS_IOS_OPTIONS_PRIVACY_COOKIES_ALLOW_COOKIES_DESCRIPTION);
      break;
    case ItemTypeBlockThirdPartyCookiesIncognito:
      message = l10n_util::GetNSString(
          IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_INCOGNITO_DESCRIPTION);
      break;
    case ItemTypeBlockThirdPartyCookies:
      message = l10n_util::GetNSString(
          IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_DESCRIPTION);
      break;
    case ItemTypeBlockAllCookies:
      message = l10n_util::GetNSString(
          IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_ALL_COOKIES_DESCRIPTION);
      break;
    default:
      return;
  }

  PopoverLabelViewController* bubbleViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = button;
  bubbleViewController.popoverPresentationController.sourceRect = button.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionUp;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  ItemType itemType =
      (ItemType)[self.tableViewModel itemTypeForIndexPath:indexPath];
  CookiesSettingType settingType;
  switch (itemType) {
    case ItemTypeAllowCookies:
      settingType = SettingTypeAllowCookies;
      break;
    case ItemTypeBlockThirdPartyCookiesIncognito:
      settingType = SettingTypeBlockThirdPartyCookiesIncognito;
      break;
    case ItemTypeBlockThirdPartyCookies:
      settingType = SettingTypeBlockThirdPartyCookies;
      break;
    case ItemTypeBlockAllCookies:
      settingType = SettingTypeBlockAllCookies;
      break;
    default:
      return;
  }
  [self.handler selectedCookiesSettingType:settingType];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeCookiesDescriptionFooter)
    return cell;

  TableViewInfoButtonCell* managedCell =
      base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
  [managedCell.trailingButton addTarget:self
                                 action:@selector(didTapInfoIcon:)
                       forControlEvents:UIControlEventTouchUpInside];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeAllowCookies: {
      managedCell.trailingButton.tag = ItemTypeAllowCookies;
      break;
    }
    case ItemTypeBlockThirdPartyCookiesIncognito: {
      managedCell.trailingButton.tag = ItemTypeBlockThirdPartyCookiesIncognito;
      break;
    }
    case ItemTypeBlockThirdPartyCookies: {
      managedCell.trailingButton.tag = ItemTypeBlockThirdPartyCookies;
      break;
    }

    case ItemTypeBlockAllCookies: {
      managedCell.trailingButton.tag = ItemTypeBlockAllCookies;
      break;
    }
  }
  return cell;
}

#pragma mark - PrivacyCookiesConsumer

- (void)cookiesSettingsOptionSelected:(CookiesSettingType)settingType {
  self.selectedSetting = [self itemTypeForCookiesSettingType:settingType];
  NSIndexPath* indexPath = [self.tableViewModel
      indexPathForItemType:[self itemTypeForCookiesSettingType:settingType]];
  [self updateSelectedCookiesItemWithIndexPath:indexPath];
}

@end
