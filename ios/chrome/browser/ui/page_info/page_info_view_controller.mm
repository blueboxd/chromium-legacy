// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/content_settings/core/common/features.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSecurityContent = kSectionIdentifierEnumZero,
  SectionIdentifierCookiesContent
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSecurityHeader = kItemTypeEnumZero,
  ItemTypeSecurityDescription,
  ItemTypeCookiesHeader,
  ItemTypeCookiesSwitch,
  ItemTypeCookiesBlocked,
  ItemTypeCookiesInUse,
  ItemTypeCookiesClear,
  ItemTypeCookiesDescription,
};

}  // namespace

@interface PageInfoViewController () <TableViewTextLinkCellDelegate>

@property(nonatomic, strong)
    PageInfoSiteSecurityDescription* pageInfoSecurityDescription;

@end

@implementation PageInfoViewController

#pragma mark - UIViewController

- (instancetype)initWithSiteSecurityDescription:
    (PageInfoSiteSecurityDescription*)description {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _pageInfoSecurityDescription = description;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_INFORMATION);
  self.navigationItem.prompt = self.pageInfoSecurityDescription.siteURL;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.handler
                           action:@selector(hidePageInfo)];
  self.navigationItem.rightBarButtonItem = dismissButton;

  if (self.pageInfoSecurityDescription.isEmpty) {
    [self addEmptyTableViewWithMessage:self.pageInfoSecurityDescription.message
                                 image:nil];
    self.tableView.alwaysBounceVertical = NO;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    return;
  }

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierSecurityContent];

  TableViewDetailIconItem* securityHeader =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeSecurityHeader];
  securityHeader.text = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_SECURITY);
  securityHeader.detailText = self.pageInfoSecurityDescription.status;
  securityHeader.iconImageName = self.pageInfoSecurityDescription.iconImageName;
  [self.tableViewModel addItem:securityHeader
       toSectionWithIdentifier:SectionIdentifierSecurityContent];

  TableViewTextLinkItem* securityDescription =
      [[TableViewTextLinkItem alloc] initWithType:ItemTypeSecurityDescription];
  securityDescription.text = self.pageInfoSecurityDescription.message;
  securityDescription.linkURL = GURL(kPageInfoHelpCenterURL);
  [self.tableViewModel addItem:securityDescription
       toSectionWithIdentifier:SectionIdentifierSecurityContent];

  if (base::FeatureList::IsEnabled(content_settings::kImprovedCookieControls))
    [self loadCookiesModel];
}

#pragma mark - Private

// Adds Items to the tableView related to Cookies Settings.
- (void)loadCookiesModel {
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewTextHeaderFooterItem* cookiesHeader =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:ItemTypeSecurityDescription];
  cookiesHeader.text = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_COOKIES_HEADER);
  [self.tableViewModel setHeader:cookiesHeader
        forSectionWithIdentifier:SectionIdentifierCookiesContent];

  // TODO(crbug.com/1038919): Implement this.
  SettingsSwitchItem* cookiesSwitchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeCookiesSwitch];
  cookiesSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_COOKIES_SWITCH_LABEL);
  [self.tableViewModel addItem:cookiesSwitchItem
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  // TODO(crbug.com/1038919): Implement this.
  TableViewMultiDetailTextItem* cookiesBlocked =
      [[TableViewMultiDetailTextItem alloc]
          initWithType:ItemTypeCookiesBlocked];
  cookiesBlocked.text =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_COOKIES_BLOCKED_LABEL);
  // TODO(crbug.com/1038919): Remove this.
  cookiesBlocked.trailingDetailText = @"2";
  [self.tableViewModel addItem:cookiesBlocked
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  // TODO(crbug.com/1038919): Implement this.
  TableViewMultiDetailTextItem* cookiesInUse =
      [[TableViewMultiDetailTextItem alloc] initWithType:ItemTypeCookiesInUse];
  cookiesInUse.text =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_COOKIES_IN_USE_LABEL);
  // TODO(crbug.com/1038919): Remove this.
  cookiesInUse.trailingDetailText = @"2";
  [self.tableViewModel addItem:cookiesInUse
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewTextItem* cookiesClear =
      [[TableViewTextItem alloc] initWithType:ItemTypeCookiesClear];
  cookiesClear.text =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_COOKIES_CLEAR_SITE_LABEL);
  cookiesClear.textColor = [UIColor colorNamed:kRedColor];
  [self.tableViewModel addItem:cookiesClear
       toSectionWithIdentifier:SectionIdentifierCookiesContent];

  TableViewTextLinkItem* cookiesDescription =
      [[TableViewTextLinkItem alloc] initWithType:ItemTypeCookiesDescription];
  cookiesDescription.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_COOKIES_DESCRIPTION);
  [self.tableViewModel addItem:cookiesDescription
       toSectionWithIdentifier:SectionIdentifierCookiesContent];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn = [super tableView:tableView
                             cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];

  if (item.type == ItemTypeSecurityDescription) {
    TableViewTextLinkCell* tableViewTextLinkCell =
        base::mac::ObjCCastStrict<TableViewTextLinkCell>(cellToReturn);
    tableViewTextLinkCell.delegate = self;
  }

  return cellToReturn;
}

#pragma mark - TableViewTextLinkCellDelegate

- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(const GURL&)URL {
  [self.handler showSecurityHelpPage];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  if (itemType != ItemTypeCookiesClear)
    return;
  // TODO(crbug.com/1038919): Implement this.
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView willSelectRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  if (itemType == ItemTypeCookiesClear)
    return indexPath;

  return nil;
}

@end
