// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/handoff_table_view_controller.h"

#import "base/mac/foundation_util.h"
#include "components/handoff/pref_names_ios.h"
#include "components/prefs/pref_member.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitch = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface HandoffTableViewController () {
  // Pref for whether Handoff is enabled.
  BooleanPrefMember _handoffEnabled;
}

- (void)switchChanged:(UISwitch*)switchView;

@end

@implementation HandoffTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithStyle:style];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_CONTINUITY_LABEL);
    _handoffEnabled.Init(prefs::kIosHandoffToOtherDevices,
                         browserState->GetPrefs());
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedSectionFooterHeight = 70;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitch];
  SettingsSwitchItem* switchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES);
  switchItem.on = _handoffEnabled.GetValue();
  [model addItem:switchItem toSectionWithIdentifier:SectionIdentifierSwitch];

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES_DETAILS);
  [model setFooter:footer forSectionWithIdentifier:SectionIdentifierSwitch];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeSwitch) {
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - Private

- (void)switchChanged:(UISwitch*)switchView {
  _handoffEnabled.SetValue(switchView.isOn);
}

@end
