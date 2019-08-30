// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy_table_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/google/core/common/google_util.h"
#include "components/handoff/pref_names_ios.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/ios/features.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_local_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/handoff_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
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
  ItemTypeWebServicesPaymentSwitch,
  ItemTypeWebServicesSendUsageData,
  ItemTypeWebServicesShowSuggestions,
  ItemTypeWebServicesFooter,
  ItemTypeClearBrowsingDataClear,
  // Footer to suggest the user to open Sync and Google services settings.
  ItemTypeClearBrowsingDataFooter,
};

// Only used in this class to openn the Sync and Google services settings.
// This link should not be dispatched.
GURL kGoogleServicesSettingsURL("settings://open_google_services");

}  // namespace

@interface PrivacyTableViewController () <BooleanObserver,
                                          ClearBrowsingDataLocalCommands,
                                          PrefObserverDelegate> {
  ios::ChromeBrowserState* _browserState;  // weak
  PrefBackedBoolean* _suggestionsEnabled;
  // The item related to the switch for the show suggestions setting.
  SettingsSwitchItem* _showSuggestionsItem;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  PrefChangeRegistrar _prefChangeRegistrarApplicationContext;

  // Updatable Items
  TableViewDetailIconItem* _handoffDetailItem;
  TableViewDetailIconItem* _sendUsageDetailItem;
  SettingsSwitchItem* _sendUsageToggleSwitchItem;
}

@end

@implementation PrivacyTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _browserState = browserState;
    self.title =
        l10n_util::GetNSString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);
    if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
      // When unified consent flag is enabled, the suggestion setting is
      // available in the "Google Services and sync" settings.
      _suggestionsEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_browserState->GetPrefs()
                     prefName:prefs::kSearchSuggestEnabled];
      [_suggestionsEnabled setObserver:self];
    }

    PrefService* prefService = _browserState->GetPrefs();

    _prefChangeRegistrar.Init(prefService);
    _prefChangeRegistrarApplicationContext.Init(
        GetApplicationContext()->GetLocalState());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosHandoffToOtherDevices, &_prefChangeRegistrar);
    if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
      _prefObserverBridge->ObserveChangesForPreference(
          metrics::prefs::kMetricsReportingEnabled,
          &_prefChangeRegistrarApplicationContext);
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kMetricsReportingWifiOnly,
          &_prefChangeRegistrarApplicationContext);
    }
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
  [model addItem:[self canMakePaymentItem]
      toSectionWithIdentifier:SectionIdentifierWebServices];
  if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // When unified consent flag is enabled, the show suggestions feature and
    // metrics reporting feature are available in the "Google Services and sync"
    // settings.
    if (base::FeatureList::IsEnabled(kUmaCellular)) {
      [model addItem:[self sendUsageToggleSwitchItem]
          toSectionWithIdentifier:SectionIdentifierWebServices];
    } else {
      [model addItem:[self sendUsageDetailItem]
          toSectionWithIdentifier:SectionIdentifierWebServices];
    }
    _showSuggestionsItem = [self showSuggestionsSwitchItem];
    [model addItem:_showSuggestionsItem
        toSectionWithIdentifier:SectionIdentifierWebServices];

    [model setFooter:[self showSuggestionsFooterItem]
        forSectionWithIdentifier:SectionIdentifierWebServices];
  }


  // Clear Browsing Section
  [model addSectionWithIdentifier:SectionIdentifierClearBrowsingData];
  [model addItem:[self clearBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierClearBrowsingData];
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    [model setFooter:[self showClearBrowsingDataFooterItem]
        forSectionWithIdentifier:SectionIdentifierClearBrowsingData];
  }
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

- (SettingsSwitchItem*)showSuggestionsSwitchItem {
  SettingsSwitchItem* showSuggestionsSwitchItem = [[SettingsSwitchItem alloc]
      initWithType:ItemTypeWebServicesShowSuggestions];
  showSuggestionsSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_SEARCH_URL_SUGGESTIONS);
  showSuggestionsSwitchItem.on = [_suggestionsEnabled value];

  return showSuggestionsSwitchItem;
}

- (TableViewHeaderFooterItem*)showSuggestionsFooterItem {
  TableViewLinkHeaderFooterItem* showSuggestionsFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeWebServicesFooter];
  showSuggestionsFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_FOOTER);
  showSuggestionsFooterItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(kPrivacyLearnMoreURL),
      GetApplicationContext()->GetApplicationLocale());

  return showSuggestionsFooterItem;
}

// Creates TableViewHeaderFooterItem instance to show a link to open the Sync
// and Google services settings.
- (TableViewHeaderFooterItem*)showClearBrowsingDataFooterItem {
  TableViewLinkHeaderFooterItem* showClearBrowsingDataFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeClearBrowsingDataFooter];
  showClearBrowsingDataFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_GOOGLE_SERVICES_FOOTER);
  showClearBrowsingDataFooterItem.linkURL = kGoogleServicesSettingsURL;

  return showClearBrowsingDataFooterItem;
}

- (TableViewItem*)clearBrowsingDetailItem {
  return [self detailItemWithType:ItemTypeClearBrowsingDataClear
                          titleId:IDS_IOS_CLEAR_BROWSING_DATA_TITLE
                       detailText:nil];
}

- (TableViewItem*)canMakePaymentItem {
  SettingsSwitchItem* canMakePaymentItem = [[SettingsSwitchItem alloc]
      initWithType:ItemTypeWebServicesPaymentSwitch];
  canMakePaymentItem.text =
      l10n_util::GetNSString(IDS_SETTINGS_CAN_MAKE_PAYMENT_TOGGLE_LABEL);
  canMakePaymentItem.on = [self isCanMakePaymentEnabled];
  return canMakePaymentItem;
}

- (BOOL)isCanMakePaymentEnabled {
  return _browserState->GetPrefs()->GetBoolean(
      payments::kCanMakePaymentEnabled);
}

- (void)setCanMakePaymentEnabled:(BOOL)isEnabled {
  _browserState->GetPrefs()->SetBoolean(payments::kCanMakePaymentEnabled,
                                        isEnabled);
}

- (TableViewItem*)sendUsageDetailItem {
  NSString* detailText = [DataplanUsageTableViewController
      currentLabelForPreference:GetApplicationContext()->GetLocalState()
                       basePref:metrics::prefs::kMetricsReportingEnabled
                       wifiPref:prefs::kMetricsReportingWifiOnly];
  _sendUsageDetailItem =
      [self detailItemWithType:ItemTypeWebServicesSendUsageData
                       titleId:IDS_IOS_OPTIONS_SEND_USAGE_DATA
                    detailText:detailText];

  return _sendUsageDetailItem;
}

- (SettingsSwitchItem*)sendUsageToggleSwitchItem {
  _sendUsageToggleSwitchItem = [[SettingsSwitchItem alloc]
      initWithType:ItemTypeWebServicesSendUsageData];
  _sendUsageToggleSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_SEND_USAGE_DATA);
  _sendUsageToggleSwitchItem.on =
      GetApplicationContext()->GetLocalState()->GetBoolean(
          metrics::prefs::kMetricsReportingEnabled);

  return _sendUsageToggleSwitchItem;
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

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeWebServicesShowSuggestions) {
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(showSuggestionsToggled:)
                    forControlEvents:UIControlEventValueChanged];
  } else if (itemType == ItemTypeWebServicesPaymentSwitch) {
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(canMakePaymentSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  } else if (itemType == ItemTypeWebServicesSendUsageData &&
             base::FeatureList::IsEnabled(kUmaCellular)) {
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(sendUsageDataToggled:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
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
    case ItemTypeWebServicesSendUsageData:
      if (!base::FeatureList::IsEnabled(kUmaCellular)) {
        controller = [[DataplanUsageTableViewController alloc]
            initWithPrefs:GetApplicationContext()->GetLocalState()
                 basePref:metrics::prefs::kMetricsReportingEnabled
                 wifiPref:prefs::kMetricsReportingWifiOnly
                    title:l10n_util::GetNSString(
                              IDS_IOS_OPTIONS_SEND_USAGE_DATA)];
      }
      break;
    case ItemTypeClearBrowsingDataClear:
      if (IsNewClearBrowsingDataUIEnabled()) {
        ClearBrowsingDataTableViewController* clearBrowsingDataViewController =
            [[ClearBrowsingDataTableViewController alloc]
                initWithBrowserState:_browserState];
        clearBrowsingDataViewController.localDispatcher = self;
        controller = clearBrowsingDataViewController;
      } else {
        controller = [[ClearBrowsingDataCollectionViewController alloc]
            initWithBrowserState:_browserState];
      }
      break;
    case ItemTypeWebServicesPaymentSwitch:
    case ItemTypeWebServicesShowSuggestions:
    default:
      break;
  }

  if (controller) {
    controller.dispatcher = self.dispatcher;
    [self.navigationController pushViewController:controller animated:YES];
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _suggestionsEnabled);

  // Update the item.
  _showSuggestionsItem.on = [_suggestionsEnabled value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ _showSuggestionsItem ]];
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

#pragma mark - Actions

- (void)showSuggestionsToggled:(UISwitch*)sender {
  NSIndexPath* switchPath = [self.tableViewModel
      indexPathForItemType:ItemTypeWebServicesShowSuggestions
         sectionIdentifier:SectionIdentifierWebServices];

  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  SettingsSwitchCell* switchCell =
      base::mac::ObjCCastStrict<SettingsSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  if (switchCell.switchView.isOn) {
    base::RecordAction(base::UserMetricsAction(
        "ContentSuggestions.RemoteSuggestionsPreferenceOn"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "ContentSuggestions.RemoteSuggestionsPreferenceOff"));
  }

  DCHECK_EQ(switchCell.switchView, sender);
  BOOL isOn = switchCell.switchView.isOn;
  switchItem.on = isOn;
  [_suggestionsEnabled setValue:isOn];
}

- (void)canMakePaymentSwitchChanged:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeWebServicesPaymentSwitch
                              sectionIdentifier:SectionIdentifierWebServices];

  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  SettingsSwitchCell* switchCell =
      base::mac::ObjCCastStrict<SettingsSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  DCHECK_EQ(switchCell.switchView, sender);
  switchItem.on = sender.isOn;
  [self setCanMakePaymentEnabled:sender.isOn];
}

- (void)sendUsageDataToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItem:_sendUsageToggleSwitchItem];
  SettingsSwitchCell* switchCell =
      base::mac::ObjCCastStrict<SettingsSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  DCHECK_EQ(switchCell.switchView, sender);
  _sendUsageToggleSwitchItem.on = sender.isOn;

  GetApplicationContext()->GetLocalState()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, sender.isOn);
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

  if (preferenceName == metrics::prefs::kMetricsReportingEnabled ||
      preferenceName == prefs::kMetricsReportingWifiOnly) {
    DCHECK(!unified_consent::IsUnifiedConsentFeatureEnabled());
    if (base::FeatureList::IsEnabled(kUmaCellular)) {
      bool isOn = GetApplicationContext()->GetLocalState()->GetBoolean(
          metrics::prefs::kMetricsReportingEnabled);
      _sendUsageToggleSwitchItem.on = isOn;
      [self reconfigureCellsForItems:@[ _sendUsageToggleSwitchItem ]];
      return;
    } else {
      NSString* detailText = [DataplanUsageTableViewController
          currentLabelForPreference:GetApplicationContext()->GetLocalState()
                           basePref:metrics::prefs::kMetricsReportingEnabled
                           wifiPref:prefs::kMetricsReportingWifiOnly];

      _sendUsageDetailItem.detailText = detailText;

      [self reconfigureCellsForItems:@[ _sendUsageDetailItem ]];
      return;
    }
  }
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL {
  if (URL == kGoogleServicesSettingsURL) {
    // kGoogleServicesSettingsURL is not a realy link. It should be handled
    // with a special case.
    [self.dispatcher showGoogleServicesSettingsFromViewController:self];
  } else {
    [super view:view didTapLinkURL:URL];
  }
}

@end
