// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "components/content_settings/core/common/features.h"
#include "components/handoff/pref_names_ios.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
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
  SectionIndentifierCookies,
  SectionIdentifierClearBrowsingData,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeOtherDevicesHandoff = kItemTypeEnumZero,
  ItemTypeCookies,
  ItemTypeClearBrowsingDataClear,
  // Footer to suggest the user to open Sync and Google services settings.
  ItemTypeClearBrowsingDataFooter,
};

// Only used in this class to openn the Sync and Google services settings.
// This link should not be dispatched.
const char kGoogleServicesSettingsURL[] = "settings://open_google_services";

}  // namespace

@interface PrivacyTableViewController () <PrefObserverDelegate> {
  ChromeBrowserState* _browserState;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Updatable Items
  TableViewDetailIconItem* _handoffDetailItem;
}

// Browser.
@property(nonatomic, readonly) Browser* browser;

@end

@implementation PrivacyTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _browserState = browser->GetBrowserState();
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

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate privacyTableViewControllerWasRemoved:self];
  }
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Web Services Section
  [model addSectionWithIdentifier:SectionIdentifierWebServices];
  [model addItem:[self handoffDetailItem]
      toSectionWithIdentifier:SectionIdentifierWebServices];

  if (base::FeatureList::IsEnabled(content_settings::kImprovedCookieControls)) {
    // Cookies Section
    [model addSectionWithIdentifier:SectionIndentifierCookies];
    [model addItem:[self cookiesItem]
        toSectionWithIdentifier:SectionIndentifierCookies];
  }

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
  _handoffDetailItem = [self
           detailItemWithType:ItemTypeOtherDevicesHandoff
                      titleId:IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES
                   detailText:detailText
      accessibilityIdentifier:kSettingsHandoffCellId];

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
  showClearBrowsingDataFooterItem.linkURL = GURL(kGoogleServicesSettingsURL);

  return showClearBrowsingDataFooterItem;
}

// Returns TableViewHeaderFooterItem instance to open Cookies screen.
- (TableViewItem*)cookiesItem {
  return [self detailItemWithType:ItemTypeCookies
                          titleId:IDS_IOS_OPTIONS_PRIVACY_COOKIES
                       detailText:nil
          accessibilityIdentifier:kSettingsCookiesCellId];
}

- (TableViewItem*)clearBrowsingDetailItem {
  return [self detailItemWithType:ItemTypeClearBrowsingDataClear
                          titleId:IDS_IOS_CLEAR_BROWSING_DATA_TITLE
                       detailText:nil
          accessibilityIdentifier:kSettingsClearBrowsingDataCellId];
}

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                       titleId:(NSInteger)titleId
                                    detailText:(NSString*)detailText
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = l10n_util::GetNSString(titleId);
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;

  return detailItem;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
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
  switch (itemType) {
    case ItemTypeOtherDevicesHandoff:
      [self.handler showHandoff];
      break;
    case ItemTypeClearBrowsingDataClear:
      [self.handler showClearBrowsingData];
      break;
    case ItemTypeCookies:
      [self.handler showCookies];
      break;
    default:
      break;
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
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
  if (URL == GURL(kGoogleServicesSettingsURL)) {
    // kGoogleServicesSettingsURL is not a realy link. It should be handled
    // with a special case.
    [self.dispatcher showGoogleServicesSettingsFromViewController:self];
  } else {
    [super view:view didTapLinkURL:URL];
  }
}

@end
