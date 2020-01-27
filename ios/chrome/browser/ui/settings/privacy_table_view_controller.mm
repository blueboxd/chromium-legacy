// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy_table_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "components/handoff/pref_names_ios.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_local_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/handoff_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPrivacyTableViewId = @"kPrivacyTableViewId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierWebServices = kSectionIdentifierEnumZero,
  SectionIdentifierClearBrowsingData,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeOtherDevicesHandoff = kItemTypeEnumZero,
  ItemTypeClearBrowsingDataClear,
  // Footer to suggest the user to open Sync and Google services settings.
  ItemTypeClearBrowsingDataFooter,
};

// Only used in this class to openn the Sync and Google services settings.
// This link should not be dispatched.
// TODO(crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL GoogleServicesSettingsURL() {
  return GURL("settings://open_google_services");
}

}  // namespace

@interface PrivacyTableViewController () <ClearBrowsingDataLocalCommands,
                                          PrefObserverDelegate> {
  ChromeBrowserState* _browserState;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Updatable Items
  TableViewDetailIconItem* _handoffDetailItem;
}

@end

@implementation PrivacyTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithStyle:style];
  if (self) {
    _browserState = browserState;
    self.title =
        l10n_util::GetNSString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);

    PrefService* prefService = _browserState->GetPrefs();

    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosHandoffToOtherDevices, &_prefChangeRegistrar);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPrivacyTableViewId;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Web Services Section
  [model addSectionWithIdentifier:SectionIdentifierWebServices];
  [model addItem:[self handoffDetailItem]
      toSectionWithIdentifier:SectionIdentifierWebServices];

  // Clear Browsing Section
  [model addSectionWithIdentifier:SectionIdentifierClearBrowsingData];
  [model addItem:[self clearBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierClearBrowsingData];
  [model setFooter:[self showClearBrowsingDataFooterItem]
      forSectionWithIdentifier:SectionIdentifierClearBrowsingData];
}

#pragma mark - Model Objects

- (TableViewItem*)handoffDetailItem {
  NSString* detailText =
      _browserState->GetPrefs()->GetBoolean(prefs::kIosHandoffToOtherDevices)
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _handoffDetailItem =
      [self detailItemWithType:ItemTypeOtherDevicesHandoff
                       titleId:IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES
                    detailText:detailText];

  return _handoffDetailItem;
}

// Creates TableViewHeaderFooterItem instance to show a link to open the Sync
// and Google services settings.
- (TableViewHeaderFooterItem*)showClearBrowsingDataFooterItem {
  TableViewLinkHeaderFooterItem* showClearBrowsingDataFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeClearBrowsingDataFooter];
  showClearBrowsingDataFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_GOOGLE_SERVICES_FOOTER);
  showClearBrowsingDataFooterItem.linkURL = GoogleServicesSettingsURL();

  return showClearBrowsingDataFooterItem;
}

- (TableViewItem*)clearBrowsingDetailItem {
  return [self detailItemWithType:ItemTypeClearBrowsingDataClear
                          titleId:IDS_IOS_CLEAR_BROWSING_DATA_TITLE
                       detailText:nil];
}

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                       titleId:(NSInteger)titleId
                                    detailText:(NSString*)detailText {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = l10n_util::GetNSString(titleId);
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;

  return detailItem;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView =
      [super tableView:tableView viewForFooterInSection:section];
  TableViewLinkHeaderFooterView* footer =
      base::mac::ObjCCast<TableViewLinkHeaderFooterView>(footerView);
  if (footer) {
    footer.delegate = self;
  }
  return footerView;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  // Items that push a new view controller.
  UIViewController<SettingsRootViewControlling>* controller;

  switch (itemType) {
    case ItemTypeOtherDevicesHandoff:
      controller = [[HandoffTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeClearBrowsingDataClear: {
      ClearBrowsingDataTableViewController* clearBrowsingDataViewController =
          [[ClearBrowsingDataTableViewController alloc]
              initWithBrowserState:_browserState];
      clearBrowsingDataViewController.localDispatcher = self;
      controller = clearBrowsingDataViewController;
      break;
    }
    default:
      break;
  }

  if (controller) {
    controller.dispatcher = self.dispatcher;
    [self.navigationController pushViewController:controller animated:YES];
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - ClearBrowsingDataLocalCommands

- (void)openURL:(const GURL&)URL {
  DCHECK(self.dispatcher);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)dismissClearBrowsingData {
  SettingsNavigationController* navigationController =
      base::mac::ObjCCastStrict<SettingsNavigationController>(
          self.navigationController);
  [navigationController closeSettings];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosHandoffToOtherDevices) {
    NSString* detailText =
        _browserState->GetPrefs()->GetBoolean(prefs::kIosHandoffToOtherDevices)
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _handoffDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _handoffDetailItem ]];
    return;
  }
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL {
  if (URL == GoogleServicesSettingsURL()) {
    // GoogleServicesSettingsURL() is not a realy link. It should be handled
    // with a special case.
    [self.dispatcher showGoogleServicesSettingsFromViewController:self];
  } else {
    [super view:view didTapLinkURL:URL];
  }
}

@end
