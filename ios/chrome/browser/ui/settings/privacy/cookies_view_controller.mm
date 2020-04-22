// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/cookies_view_controller.h"

#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/settings/cells/settings_multiline_detail_item.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_commands.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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
  ItemTypeSiteExceptionsHeader,
  ItemTypeSiteExceptionItem,
  ItemTypeAddSiteExceptions,
};

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCookiesContent = kSectionIdentifierEnumZero,
  SectionIdentifierSiteExceptionsContent,
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
  [self.tableView setEditing:YES animated:NO];
  self.tableView.allowsSelectionDuringEditing = YES;

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
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierSiteExceptionsContent];

  SettingsMultilineDetailItem* allowCookies =
      [[SettingsMultilineDetailItem alloc] initWithType:ItemTypeAllowCookies];
  allowCookies.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_ALLOW_COOKIES_TITLE);
  allowCookies.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_ALLOW_COOKIES_DETAIL);
  allowCookies.useCustomSeparator = YES;
  [self.tableViewModel addItem:allowCookies
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  SettingsMultilineDetailItem* blockThirdPartyCookiesIncognito =
      [[SettingsMultilineDetailItem alloc]
          initWithType:ItemTypeBlockThirdPartyCookiesIncognito];
  blockThirdPartyCookiesIncognito.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_INCOGNITO_TITLE);
  blockThirdPartyCookiesIncognito.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_DETAIL);
  blockThirdPartyCookiesIncognito.useCustomSeparator = YES;
  [self.tableViewModel addItem:blockThirdPartyCookiesIncognito
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  SettingsMultilineDetailItem* blockThirdPartyCookies =
      [[SettingsMultilineDetailItem alloc]
          initWithType:ItemTypeBlockThirdPartyCookies];
  blockThirdPartyCookies.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_TITLE);
  blockThirdPartyCookies.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_THIRD_PARTY_COOKIES_DETAIL);
  blockThirdPartyCookies.useCustomSeparator = YES;
  [self.tableViewModel addItem:blockThirdPartyCookies
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  SettingsMultilineDetailItem* blockAllCookies =
      [[SettingsMultilineDetailItem alloc]
          initWithType:ItemTypeBlockAllCookies];
  blockAllCookies.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_ALL_COOKIES_TITLE);
  blockAllCookies.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_BLOCK_ALL_COOKIES_DETAIL);
  blockAllCookies.useCustomSeparator = YES;
  [self.tableViewModel addItem:blockAllCookies
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  // This item is used as a footer. It's currently not possible to have a
  // separtor without using UITableViewStyleGrouped.
  TableViewTextLinkItem* cookiesDescriptionFooter =
      [[TableViewTextLinkItem alloc]
          initWithType:ItemTypeCookiesDescriptionFooter];
  cookiesDescriptionFooter.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_COOKIES_DESCRIPTION);
  [self.tableViewModel addItem:cookiesDescriptionFooter
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewTextHeaderFooterItem* siteExceptionHeader =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:ItemTypeSiteExceptionsHeader];
  siteExceptionHeader.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_SITE_EXCEPTIONS_HEADER);
  [self.tableViewModel setHeader:siteExceptionHeader
        forSectionWithIdentifier:SectionIdentifierSiteExceptionsContent];

  TableViewTextItem* addSiteExceptions =
      [[TableViewTextItem alloc] initWithType:ItemTypeAddSiteExceptions];
  addSiteExceptions.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_PRIVACY_COOKIES_ADD_SITE_EXCEPTION);
  addSiteExceptions.textColor = [UIColor colorNamed:kBlueColor];
  addSiteExceptions.useCustomSeparator = YES;
  [self.tableViewModel addItem:addSiteExceptions
       toSectionWithIdentifier:SectionIdentifierSiteExceptionsContent];

  // TODO(crbug.com/1064961): Implement this.
}

#pragma mark - Private

// Adds accessoryType to the selected item & cell.
// Removes the accessoryType of the prevous selected option item & cell.
- (void)updateSelectedCookiesItemWithIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* selectedCookiesItemCell;
  if (self.selectedCookiesItem) {
    self.selectedCookiesItem.accessoryType = UITableViewCellAccessoryNone;
    selectedCookiesItemCell = [self.tableView
        cellForRowAtIndexPath:[self.tableViewModel
                                  indexPathForItem:self.selectedCookiesItem]];
    selectedCookiesItemCell.accessoryType = UITableViewCellAccessoryNone;
  }
  self.selectedCookiesItem = [self.tableViewModel itemAtIndexPath:indexPath];
  self.selectedCookiesItem.accessoryType = UITableViewCellAccessoryCheckmark;
  selectedCookiesItemCell = [self.tableView
      cellForRowAtIndexPath:[self.tableViewModel
                                indexPathForItem:self.selectedCookiesItem]];
  selectedCookiesItemCell.accessoryType = UITableViewCellAccessoryCheckmark;
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
  [self updateSelectedCookiesItemWithIndexPath:indexPath];
  [self.handler selectedCookiesSettingType:settingType];
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeSiteExceptionItem;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete)
    return;

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.handler deleteSiteExceptionWithItem:item];
  [self deleteItems:@[ indexPath ]];
}

#pragma mark - PrivacyCookiesConsumer

- (void)cookiesSettingsOptionSelected:(CookiesSettingType)settingType {
  self.selectedSetting = [self itemTypeForCookiesSettingType:settingType];
}

- (void)insertSiteExceptionsItems:(NSArray<TableViewItem*>*)items {
  if (!self.tableViewModel)
    [self loadModel];

  NSInteger index = 0;
  for (TableViewItem* item in items) {
    [item setType:ItemTypeSiteExceptionItem];
    [self.tableViewModel insertItem:item
            inSectionWithIdentifier:SectionIdentifierSiteExceptionsContent
                            atIndex:index++];
  }
}

@end
