// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_settings_table_view_controller.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/mac/foundation_util.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The a11y identifier of the view controller's view.
NSString* const kSettingsSyncId = @"kSettingsSyncId";
// Notification when a switch account operation will start.
NSString* const kSwitchAccountWillStartNotification =
    @"kSwitchAccountWillStartNotification";
// Notification when a switch account operation did finish.
NSString* const kSwitchAccountDidFinishNotification =
    @"kSwitchAccountDidFinishNotification";
// Used to tag and retrieve ItemTypeSyncableDataType cell tags.
const NSInteger kTagShift = 1000;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSyncError = kSectionIdentifierEnumZero,
  SectionIdentifierEnableSync,
  SectionIdentifierSyncAccounts,
  SectionIdentifierSyncServices,
  SectionIdentifierEncryptionAndFooter,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSyncError = kItemTypeEnumZero,
  ItemTypeSyncSwitch,
  ItemTypeAccount,
  ItemTypeSyncEverything,
  ItemTypeSyncableDataType,
  ItemTypeAutofillWalletImport,
  ItemTypeEncryption,
  ItemTypeManageSyncedData,
  ItemTypeHeader,
};

}  // namespace

@interface SyncSettingsTableViewController () <
    ChromeIdentityServiceObserver,
    IdentityManagerObserverBridgeDelegate,
    SettingsControllerProtocol,
    SyncObserverModelBridge> {
  ios::ChromeBrowserState* _browserState;  // Weak.
  SyncSetupService* _syncSetupService;     // Weak.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  AuthenticationFlow* _authenticationFlow;
  // Whether switching sync account is allowed on the screen.
  BOOL _allowSwitchSyncAccount;
  // Whether an authentication operation is in progress (e.g switch accounts).
  BOOL _authenticationOperationInProgress;
  // Whether Sync State changes should be currently ignored.
  BOOL _ignoreSyncStateChanges;

  // Cache for Identity items avatar images.
  ResizedAvatarCache* _avatarCache;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, TableViewItem*>* _identityMap;
}

// Stops observing browser state services.
- (void)stopBrowserStateServiceObservers;
// Pops the view if user is signed out and not authentication operation is in
// progress. Returns YES if the view was popped.
- (BOOL)popViewIfSignedOut;

// Returns a switch item for sync, set to on if |isOn| is YES.
- (TableViewItem*)syncSwitchItem:(BOOL)isOn;
// Returns an item for sync errors other than sync encryption.
- (TableViewItem*)syncErrorItem;
// Returns a switch item for sync everything, set to on if |isOn| is YES.
- (TableViewItem*)syncEverythingSwitchItem:(BOOL)isOn;
// Returns a switch item for the syncable data type |dataType|, set to on if
// |IsDataTypePreferred| for that type returns true.
- (TableViewItem*)switchItemForDataType:
    (SyncSetupService::SyncableDatatype)dataType;
// Returns a switch item for the Autofill wallet import setting.
- (TableViewItem*)switchItemForAutofillWalletImport;
// Returns an item for Encryption.
- (TableViewItem*)encryptionCellItem;
// Returns an item to open a link to manage the synced data.
- (TableViewItem*)manageSyncedDataItem;

// Action method for sync switch.
- (void)changeSyncStatusToOn:(UISwitch*)sender;
// Action method for the sync error cell.
- (void)fixSyncErrorIfPossible;
// Action method for the account cells.
- (void)startSwitchAccountForIdentity:(ChromeIdentity*)identity
                     postSignInAction:(PostSignInAction)postSigninAction;
// Callback for switch account action method.
- (void)didSwitchAccountWithSuccess:(BOOL)success;
// Action method for the Sync Everything switch.
- (void)changeSyncEverythingStatusToOn:(UISwitch*)sender;
// Action method for the data type switches.
- (void)changeDataTypeSyncStatusToOn:(UISwitch*)sender;
// Action method for the Autofill wallet import switch.
- (void)autofillWalletImportChanged:(UISwitch*)sender;
// Action method for the encryption cell.
- (void)showEncryption;

// Updates the visual status of the screen (i.e. whether cells are enabled,
// whether errors are displayed, ...).
- (void)updateTableView;
// Ensures the Sync error cell is shown when there is an error.
- (void)updateSyncError;
// Updates the Autofill wallet import cell (i.e. whether it is enabled and on).
- (void)updateAutofillWalletImportCell;
// Ensures the encryption cell displays an error if needed.
- (void)updateEncryptionCell;
// Updates the account item so it can reflect the latest state of the identity.
- (void)updateAccountItem:(TableViewAccountItem*)item
             withIdentity:(ChromeIdentity*)identity;

// Returns whether the Sync Settings screen has an Accounts section, allowing
// users to choose to which account they want to sync to.
- (BOOL)hasAccountsSection;
// Returns whether a sync error cell should be displayed.
- (BOOL)shouldDisplaySyncError;
// Returns whether the Sync settings should be disabled because of a Sync error.
- (BOOL)shouldDisableSettingsOnSyncError;
// Returns whether an error should be displayed on the encryption cell.
- (BOOL)shouldDisplayEncryptionError;
// Returns whether the existing sync error is fixable by a user action.
- (BOOL)isSyncErrorFixableByUserAction;
// Returns the ID to use to access the l10n string for the given data type.
- (int)titleIdForSyncableDataType:(SyncSetupService::SyncableDatatype)datatype;
// Returns whether the encryption item should be enabled.
- (BOOL)shouldEncryptionItemBeEnabled;
// Returns whether the sync everything item should be enabled.
- (BOOL)shouldSyncEverythingItemBeEnabled;
// Returns whether the data type items should be enabled.
- (BOOL)shouldSyncableItemsBeEnabled;
// Returns the tag for a data type switch based on its index path.
- (NSInteger)tagForIndexPath:(NSIndexPath*)indexPath;
// Returns the indexPath for a data type switch based on its tag.
- (NSIndexPath*)indexPathForTag:(NSInteger)shiftedTag;

// Whether the Autofill wallet import item should be enabled.
@property(nonatomic, readonly, getter=isAutofillWalletImportItemEnabled)
    BOOL autofillWalletImportItemEnabled;

// Whether the Autofill wallet import item should be on.
@property(nonatomic, assign, getter=isAutofillWalletImportOn)
    BOOL autofillWalletImportOn;

@end

@implementation SyncSettingsTableViewController

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
              allowSwitchSyncAccount:(BOOL)allowSwitchSyncAccount {
  DCHECK(!unified_consent::IsUnifiedConsentFeatureEnabled());
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _allowSwitchSyncAccount = allowSwitchSyncAccount;
    _browserState = browserState;
    _syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(_browserState);
    self.title = l10n_util::GetNSString(IDS_IOS_SYNC_SETTING_TITLE);
    syncer::SyncService* syncService =
        ProfileSyncServiceFactory::GetForBrowserState(_browserState);
    _syncObserver.reset(new SyncObserverBridge(self, syncService));
    _identityManagerObserver.reset(new signin::IdentityManagerObserverBridge(
        IdentityManagerFactory::GetForBrowserState(_browserState), self));
    _avatarCache = [[ResizedAvatarCache alloc] init];
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));
  }
  return self;
}

- (void)stopBrowserStateServiceObservers {
  _syncObserver.reset();
  _identityManagerObserver.reset();
  _identityServiceObserver.reset();
}

- (BOOL)popViewIfSignedOut {
  if (AuthenticationServiceFactory::GetForBrowserState(_browserState)
          ->IsAuthenticated()) {
    return NO;
  }
  if (_authenticationOperationInProgress) {
    // The signed out state might be temporary (e.g. account switch, ...).
    // Don't pop this view based on intermediary values.
    return NO;
  }
  [self.navigationController popToViewController:self animated:NO];
  [base::mac::ObjCCastStrict<SettingsNavigationController>(
      self.navigationController) popViewControllerOrCloseSettingsAnimated:NO];
  return YES;
}

#pragma mark View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kSettingsSyncId;
  [self loadModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateEncryptionCell];
}

#pragma mark SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  // SyncError section.
  if ([self shouldDisplaySyncError]) {
    [model addSectionWithIdentifier:SectionIdentifierSyncError];
    [model addItem:[self syncErrorItem]
        toSectionWithIdentifier:SectionIdentifierSyncError];
  }

  // Sync Section.
  BOOL syncEnabled = _syncSetupService->IsSyncEnabled();
  [model addSectionWithIdentifier:SectionIdentifierEnableSync];
  [model addItem:[self syncSwitchItem:syncEnabled]
      toSectionWithIdentifier:SectionIdentifierEnableSync];

  // Sync to Section.
  if ([self hasAccountsSection]) {
    NSMutableDictionary<NSString*, TableViewItem*>* mutableIdentityMap =
        [[NSMutableDictionary alloc] init];
    // Accounts section. Cells enabled if sync is on.
    [model addSectionWithIdentifier:SectionIdentifierSyncAccounts];
    TableViewTextHeaderFooterItem* syncToHeader =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    syncToHeader.text = l10n_util::GetNSString(IDS_IOS_SYNC_TO_TITLE);
    [model setHeader:syncToHeader
        forSectionWithIdentifier:SectionIdentifierSyncAccounts];
    auto* identity_manager =
        IdentityManagerFactory::GetForBrowserState(_browserState);

    for (const CoreAccountInfo& account :
         identity_manager->GetAccountsWithRefreshTokens()) {
      ChromeIdentity* identity = ios::GetChromeBrowserProvider()
                                     ->GetChromeIdentityService()
                                     ->GetIdentityWithGaiaID(account.gaia);
      TableViewItem* accountItem = [self accountItem:identity];
      [model addItem:accountItem
          toSectionWithIdentifier:SectionIdentifierSyncAccounts];
      [mutableIdentityMap setObject:accountItem forKey:identity.gaiaID];
    }
    _identityMap = mutableIdentityMap;
  }

  // Data Types to sync. Enabled if sync is on.
  [model addSectionWithIdentifier:SectionIdentifierSyncServices];
  TableViewTextHeaderFooterItem* syncServicesHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  syncServicesHeader.text =
      l10n_util::GetNSString(IDS_IOS_SYNC_DATA_TYPES_TITLE);
  [model setHeader:syncServicesHeader
      forSectionWithIdentifier:SectionIdentifierSyncServices];
  BOOL syncEverythingEnabled = _syncSetupService->IsSyncingAllDataTypes();
  [model addItem:[self syncEverythingSwitchItem:syncEverythingEnabled]
      toSectionWithIdentifier:SectionIdentifierSyncServices];
  // Specific Data Types to sync. Enabled if Sync Everything is off.
  for (int i = 0; i < SyncSetupService::kNumberOfSyncableDatatypes; ++i) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(i);
    [model addItem:[self switchItemForDataType:dataType]
        toSectionWithIdentifier:SectionIdentifierSyncServices];
  }
  // Autofill wallet import switch.
  [model addItem:[self switchItemForAutofillWalletImport]
      toSectionWithIdentifier:SectionIdentifierSyncServices];

  // Encryption section.  Enabled if sync is on.
  [model addSectionWithIdentifier:SectionIdentifierEncryptionAndFooter];
  [model addItem:[self encryptionCellItem]
      toSectionWithIdentifier:SectionIdentifierEncryptionAndFooter];
  [model addItem:[self manageSyncedDataItem]
      toSectionWithIdentifier:SectionIdentifierEncryptionAndFooter];
}

#pragma mark - Model items

- (TableViewItem*)syncSwitchItem:(BOOL)isOn {
  SyncSwitchItem* syncSwitchItem = [self
      switchItemWithType:ItemTypeSyncSwitch
                   title:l10n_util::GetNSString(IDS_IOS_SYNC_SETTING_TITLE)
                subTitle:l10n_util::GetNSString(
                             IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE)];
  syncSwitchItem.on = isOn;
  return syncSwitchItem;
}

- (TableViewItem*)syncErrorItem {
  DCHECK([self shouldDisplaySyncError]);
  TableViewAccountItem* syncErrorItem =
      [[TableViewAccountItem alloc] initWithType:ItemTypeSyncError];
  syncErrorItem.text = l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  syncErrorItem.image = [UIImage imageNamed:@"settings_error"];
  syncErrorItem.detailText = GetSyncErrorMessageForBrowserState(_browserState);
  return syncErrorItem;
}

- (TableViewItem*)accountItem:(ChromeIdentity*)identity {
  TableViewAccountItem* identityAccountItem =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  [self updateAccountItem:identityAccountItem withIdentity:identity];

  identityAccountItem.mode = _syncSetupService->IsSyncEnabled()
                                 ? TableViewAccountModeEnabled
                                 : TableViewAccountModeDisabled;
  ChromeIdentity* authenticatedIdentity =
      AuthenticationServiceFactory::GetForBrowserState(_browserState)
          ->GetAuthenticatedIdentity();
  if (identity == authenticatedIdentity) {
    identityAccountItem.accessoryType = UITableViewCellAccessoryCheckmark;
  }
  return identityAccountItem;
}

- (TableViewItem*)syncEverythingSwitchItem:(BOOL)isOn {
  SyncSwitchItem* syncSwitchItem = [self
      switchItemWithType:ItemTypeSyncEverything
                   title:l10n_util::GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE)
                subTitle:nil];
  syncSwitchItem.on = isOn;
  syncSwitchItem.enabled = [self shouldSyncEverythingItemBeEnabled];
  return syncSwitchItem;
}

- (TableViewItem*)switchItemForDataType:
    (SyncSetupService::SyncableDatatype)dataType {
  syncer::ModelType modelType = _syncSetupService->GetModelType(dataType);
  BOOL isOn = _syncSetupService->IsDataTypePreferred(modelType);

  SyncSwitchItem* syncDataTypeItem =
      [self switchItemWithType:ItemTypeSyncableDataType
                         title:l10n_util::GetNSString(
                                   [self titleIdForSyncableDataType:dataType])
                      subTitle:nil];
  syncDataTypeItem.dataType = dataType;
  syncDataTypeItem.on = isOn;
  syncDataTypeItem.enabled = [self shouldSyncableItemsBeEnabled];
  return syncDataTypeItem;
}

- (TableViewItem*)switchItemForAutofillWalletImport {
  NSString* title = l10n_util::GetNSString(
      IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL);
  SyncSwitchItem* autofillWalletImportItem =
      [self switchItemWithType:ItemTypeAutofillWalletImport
                         title:title
                      subTitle:nil];
  autofillWalletImportItem.on = [self isAutofillWalletImportOn];
  autofillWalletImportItem.enabled = [self isAutofillWalletImportItemEnabled];
  return autofillWalletImportItem;
}

- (TableViewItem*)encryptionCellItem {
  TableViewImageItem* encryptionCellItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeEncryption];
  encryptionCellItem.title =
      l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE);
  encryptionCellItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  encryptionCellItem.image = [self shouldDisplayEncryptionError]
                                 ? [UIImage imageNamed:@"settings_error"]
                                 : nil;
  encryptionCellItem.enabled = [self shouldEncryptionItemBeEnabled];
  encryptionCellItem.textColor =
      encryptionCellItem.enabled
          ? UIColor.blackColor
          : UIColorFromRGB(kTableViewSecondaryLabelLightGrayTextColor);
  return encryptionCellItem;
}

- (TableViewItem*)manageSyncedDataItem {
  TableViewTextItem* manageSyncedDataItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeManageSyncedData];
  manageSyncedDataItem.text =
      l10n_util::GetNSString(IDS_IOS_SYNC_RESET_GOOGLE_DASHBOARD_NO_LINK);
  manageSyncedDataItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return manageSyncedDataItem;
}

#pragma mark Item Constructors

- (SyncSwitchItem*)switchItemWithType:(NSInteger)type
                                title:(NSString*)title
                             subTitle:(NSString*)detailText {
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.detailText = detailText;
  return switchItem;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeSyncError: {
      TableViewAccountCell* accountCell =
          base::mac::ObjCCastStrict<TableViewAccountCell>(cell);
      accountCell.textLabel.textColor = UIColor.redColor;
      accountCell.detailTextLabel.numberOfLines = 1;
      break;
    }
    case ItemTypeSyncSwitch: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(changeSyncStatusToOn:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncEverything: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView
                 addTarget:self
                    action:@selector(changeSyncEverythingStatusToOn:)
          forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncableDataType: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(changeDataTypeSyncStatusToOn:)
                      forControlEvents:UIControlEventValueChanged];
      switchCell.switchView.tag = [self tagForIndexPath:indexPath];
      break;
    }
    case ItemTypeAutofillWalletImport: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(autofillWalletImportChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    default:
      break;
  }
  return cell;
}

#pragma mark UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSyncError:
      [self fixSyncErrorIfPossible];
      break;
    case ItemTypeAccount: {
      TableViewAccountItem* accountItem =
          base::mac::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      if (!accountItem.accessoryType) {
        [self startSwitchAccountForIdentity:accountItem.chromeIdentity
                           postSignInAction:POST_SIGNIN_ACTION_NONE];
      }
      break;
    }
    case ItemTypeEncryption:
      [self showEncryption];
      break;
    case ItemTypeManageSyncedData: {
      GURL learnMoreUrl = google_util::AppendGoogleLocaleParam(
          GURL(kSyncGoogleDashboardURL),
          GetApplicationContext()->GetApplicationLocale());
      OpenNewTabCommand* command =
          [OpenNewTabCommand commandWithURLFromChrome:learnMoreUrl];
      [self.dispatcher closeSettingsUIAndOpenURL:command];
      break;
    }
    default:
      break;
  }
}

#pragma mark - Actions

- (void)changeSyncStatusToOn:(UISwitch*)sender {
  if (self.navigationController.topViewController != self) {
    // Workaround for timing issue when taping on a switch and the error or
    // encryption cells. See crbug.com/647678.
    return;
  }

  BOOL isOn = sender.isOn;
  BOOL wasOn = _syncSetupService->IsSyncEnabled();
  if (wasOn == isOn)
    return;

  base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
  _syncSetupService->SetSyncEnabled(isOn);

  BOOL isNowOn = _syncSetupService->IsSyncEnabled();
  if (isNowOn == wasOn) {
    DLOG(WARNING) << "Call to SetSyncEnabled(" << (isOn ? "YES" : "NO")
                  << ") failed.";
    // This shouldn't happen, but in case there was an underlying sync problem,
    // make sure the UI reflects sync's reality.
    NSIndexPath* indexPath =
        [self.tableViewModel indexPathForItemType:ItemTypeSyncSwitch
                                sectionIdentifier:SectionIdentifierEnableSync];

    SyncSwitchItem* item = base::mac::ObjCCastStrict<SyncSwitchItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    item.on = isNowOn;
  }
  [self updateTableView];
}

- (void)fixSyncErrorIfPossible {
  if (![self isSyncErrorFixableByUserAction] || ![self shouldDisplaySyncError])
    return;

  // Unrecoverable errors are special-cased to only do the signing out and back
  // in from the Sync settings screen (where user interaction can safely be
  // prevented).
  if (_syncSetupService->GetSyncServiceState() ==
      SyncSetupService::kSyncServiceUnrecoverableError) {
    ChromeIdentity* authenticatedIdentity =
        AuthenticationServiceFactory::GetForBrowserState(_browserState)
            ->GetAuthenticatedIdentity();
    [self startSwitchAccountForIdentity:authenticatedIdentity
                       postSignInAction:POST_SIGNIN_ACTION_START_SYNC];
    return;
  }

  SyncSetupService::SyncServiceState syncState =
      GetSyncStateForBrowserState(_browserState);
  if (ShouldShowSyncSignin(syncState)) {
    [self.dispatcher
                showSignin:[[ShowSigninCommand alloc]
                               initWithOperation:
                                   AUTHENTICATION_OPERATION_REAUTHENTICATE
                                     accessPoint:signin_metrics::AccessPoint::
                                                     ACCESS_POINT_UNKNOWN]
        baseViewController:self];
  } else if (ShouldShowSyncSettings(syncState)) {
    if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
      [self.dispatcher showGoogleServicesSettingsFromViewController:self];
    } else {
      [self.dispatcher showSyncSettingsFromViewController:self];
    }
  } else if (ShouldShowSyncPassphraseSettings(syncState)) {
    [self.dispatcher showSyncPassphraseSettingsFromViewController:self];
  }
}

- (void)startSwitchAccountForIdentity:(ChromeIdentity*)identity
                     postSignInAction:(PostSignInAction)postSignInAction {
  if (!_syncSetupService->IsSyncEnabled())
    return;

  _authenticationOperationInProgress = YES;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kSwitchAccountWillStartNotification
                    object:self];
  [self preventUserInteraction];
  DCHECK(!_authenticationFlow);
  _authenticationFlow = [[AuthenticationFlow alloc]
          initWithBrowserState:_browserState
                      identity:identity
               shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
              postSignInAction:postSignInAction
      presentingViewController:self];
  _authenticationFlow.dispatcher = self.dispatcher;

  __weak SyncSettingsTableViewController* weakSelf = self;
  [_authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf didSwitchAccountWithSuccess:success];
  }];
}

- (void)didSwitchAccountWithSuccess:(BOOL)success {
  _authenticationFlow = nil;
  [self allowUserInteraction];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kSwitchAccountDidFinishNotification
                    object:self];
  _authenticationOperationInProgress = NO;
  if (![self popViewIfSignedOut]) {
    // Only reload the view if it wasn't popped.
    [self reloadData];
  }
}

- (void)changeSyncEverythingStatusToOn:(UISwitch*)sender {
  if (!_syncSetupService->IsSyncEnabled() ||
      [self shouldDisableSettingsOnSyncError])
    return;
  BOOL isOn = sender.isOn;
  BOOL wasOn = _syncSetupService->IsSyncingAllDataTypes();
  if (wasOn == isOn)
    return;

  base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
  _syncSetupService->SetSyncingAllDataTypes(isOn);

  // Base the UI on the actual sync value, not the toggle.
  BOOL isNowOn = _syncSetupService->IsSyncingAllDataTypes();
  if (isNowOn == wasOn) {
    DLOG(WARNING) << "Call to SetSyncingAllDataTypes(" << (isOn ? "YES" : "NO")
                  << ") failed";
    // No change - there was a sync-level problem that didn't allow the change.
    // This really shouldn't happen, but just in case, make sure the UI reflects
    // sync's reality.
    NSIndexPath* indexPath = [self.tableViewModel
        indexPathForItemType:ItemTypeSyncEverything
           sectionIdentifier:SectionIdentifierSyncServices];
    SyncSwitchItem* item = base::mac::ObjCCastStrict<SyncSwitchItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    item.on = isNowOn;
  }
  [self updateTableView];
}

- (void)changeDataTypeSyncStatusToOn:(UISwitch*)sender {
  if (!_syncSetupService->IsSyncEnabled() ||
      _syncSetupService->IsSyncingAllDataTypes() ||
      [self shouldDisableSettingsOnSyncError])
    return;

  BOOL isOn = sender.isOn;

  SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
      [self.tableViewModel itemAtIndexPath:[self indexPathForTag:sender.tag]]);
  SyncSetupService::SyncableDatatype dataType =
      (SyncSetupService::SyncableDatatype)syncSwitchItem.dataType;
  syncer::ModelType modelType = _syncSetupService->GetModelType(dataType);

  base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
  _syncSetupService->SetDataTypeEnabled(modelType, isOn);

  // Set value of Autofill wallet import accordingly if Autofill Sync changed.
  if (dataType == SyncSetupService::kSyncAutofill) {
    [self setAutofillWalletImportOn:isOn];
    [self updateTableView];
  }
}

- (void)autofillWalletImportChanged:(UISwitch*)sender {
  if (![self isAutofillWalletImportItemEnabled])
    return;

  [self setAutofillWalletImportOn:sender.isOn];
}

- (void)showEncryption {
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(_browserState);
  if (!syncService->IsEngineInitialized() ||
      !_syncSetupService->IsSyncEnabled() ||
      [self shouldDisableSettingsOnSyncError])
    return;

  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowserState:_browserState];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowserState:_browserState];
  }
  controllerToPush.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controllerToPush animated:YES];
}

#pragma mark Updates

- (void)updateTableView {
  __weak SyncSettingsTableViewController* weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        [weakSelf updateTableViewInternal];
      }
               completion:nil];
}

- (void)updateTableViewInternal {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeSyncSwitch
                              sectionIdentifier:SectionIdentifierEnableSync];

  SyncSwitchItem* syncItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  syncItem.on = _syncSetupService->IsSyncEnabled();
  [self reconfigureCellsForItems:@[ syncItem ]];

  // Update Sync Accounts section.
  if ([self hasAccountsSection]) {
    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierSyncAccounts];
    NSInteger itemsCount = [self.tableViewModel numberOfItemsInSection:section];
    NSMutableArray* accountsToReconfigure = [[NSMutableArray alloc] init];
    for (NSInteger item = 0; item < itemsCount; ++item) {
      NSIndexPath* indexPath = [self.tableViewModel
          indexPathForItemType:ItemTypeAccount
             sectionIdentifier:SectionIdentifierSyncAccounts
                       atIndex:item];
      TableViewAccountItem* accountItem =
          base::mac::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      accountItem.mode = _syncSetupService->IsSyncEnabled()
                             ? TableViewAccountModeEnabled
                             : TableViewAccountModeDisabled;
      [accountsToReconfigure addObject:accountItem];
    }
    [self reconfigureCellsForItems:accountsToReconfigure];
  }

  // Update Sync Services section.
  indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeSyncEverything
                              sectionIdentifier:SectionIdentifierSyncServices];
  SyncSwitchItem* syncEverythingItem =
      base::mac::ObjCCastStrict<SyncSwitchItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  syncEverythingItem.on = _syncSetupService->IsSyncingAllDataTypes();
  syncEverythingItem.enabled = [self shouldSyncEverythingItemBeEnabled];
  [self reconfigureCellsForItems:@[ syncEverythingItem ]];

  // Syncable data types cells
  NSMutableArray* switchsToReconfigure = [[NSMutableArray alloc] init];
  for (NSInteger index = 0;
       index < SyncSetupService::kNumberOfSyncableDatatypes; ++index) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(index);
    NSIndexPath* indexPath =
        [self.tableViewModel indexPathForItemType:ItemTypeSyncableDataType
                                sectionIdentifier:SectionIdentifierSyncServices
                                          atIndex:index];
    SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    DCHECK_EQ(index, syncSwitchItem.dataType);
    syncer::ModelType modelType = _syncSetupService->GetModelType(dataType);
    syncSwitchItem.on = _syncSetupService->IsDataTypePreferred(modelType);
    syncSwitchItem.enabled = [self shouldSyncableItemsBeEnabled];
    [switchsToReconfigure addObject:syncSwitchItem];
  }
  [self reconfigureCellsForItems:switchsToReconfigure];

  // Update Autofill wallet import cell.
  [self updateAutofillWalletImportCell];

  // Update Encryption cell.
  [self updateEncryptionCell];

  // Add/Remove the Sync Error. This is the only update that can change index
  // paths. It is done last because self.tableViewModel isn't aware of
  // the performBatchUpdates:completion: order of update/remove/delete.
  [self updateSyncError];
}

- (void)updateSyncError {
  BOOL shouldDisplayError = [self shouldDisplaySyncError];
  BOOL isDisplayingError =
      [self.tableViewModel hasItemForItemType:ItemTypeSyncError
                            sectionIdentifier:SectionIdentifierSyncError];
  if (shouldDisplayError && !isDisplayingError) {
    [self.tableViewModel insertSectionWithIdentifier:SectionIdentifierSyncError
                                             atIndex:0];
    [self.tableViewModel addItem:[self syncErrorItem]
         toSectionWithIdentifier:SectionIdentifierSyncError];
    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierSyncError];
    [self.tableView insertSections:[NSIndexSet indexSetWithIndex:section]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  } else if (!shouldDisplayError && isDisplayingError) {
    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierSyncError];
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierSyncError];
    [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

- (void)updateAutofillWalletImportCell {
  // Force turn on Autofill wallet import if syncing everthing.
  BOOL isSyncingEverything = _syncSetupService->IsSyncingAllDataTypes();
  if (isSyncingEverything) {
    [self setAutofillWalletImportOn:isSyncingEverything];
  }

  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeAutofillWalletImport
                              sectionIdentifier:SectionIdentifierSyncServices];
  SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  syncSwitchItem.on = [self isAutofillWalletImportOn];
  syncSwitchItem.enabled = [self isAutofillWalletImportItemEnabled];
  [self reconfigureCellsForItems:@[ syncSwitchItem ]];
}

- (void)updateEncryptionCell {
  BOOL shouldDisplayEncryptionError = [self shouldDisplayEncryptionError];
  NSIndexPath* indexPath = [self.tableViewModel
      indexPathForItemType:ItemTypeEncryption
         sectionIdentifier:SectionIdentifierEncryptionAndFooter];
  TableViewImageItem* item = base::mac::ObjCCastStrict<TableViewImageItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  item.image = shouldDisplayEncryptionError
                   ? [UIImage imageNamed:@"settings_error"]
                   : nil;
  item.enabled = [self shouldEncryptionItemBeEnabled];
  item.textColor =
      item.enabled ? UIColor.blackColor
                   : UIColorFromRGB(kTableViewSecondaryLabelLightGrayTextColor);
  [self reconfigureCellsForItems:@[ item ]];
}

- (void)updateAccountItem:(TableViewAccountItem*)item
             withIdentity:(ChromeIdentity*)identity {
  item.image = [_avatarCache resizedAvatarForIdentity:identity];
  item.text = identity.userEmail;
  item.chromeIdentity = identity;
}

#pragma mark Helpers

- (BOOL)hasAccountsSection {
  return _allowSwitchSyncAccount &&
         IdentityManagerFactory::GetForBrowserState(_browserState)
                 ->GetAccountsWithRefreshTokens()
                 .size() > 1;
}

- (BOOL)shouldDisplaySyncError {
  SyncSetupService::SyncServiceState state =
      _syncSetupService->GetSyncServiceState();
  // Without unity, kSyncSettingsNotConfirmed should not be shown.
  return state != SyncSetupService::kNoSyncServiceError &&
         state != SyncSetupService::kSyncSettingsNotConfirmed;
}

- (BOOL)shouldDisableSettingsOnSyncError {
  SyncSetupService::SyncServiceState state =
      _syncSetupService->GetSyncServiceState();
  return state != SyncSetupService::kNoSyncServiceError &&
         state != SyncSetupService::kSyncServiceNeedsPassphrase;
}

- (BOOL)shouldDisplayEncryptionError {
  return _syncSetupService->GetSyncServiceState() ==
         SyncSetupService::kSyncServiceNeedsPassphrase;
}

- (BOOL)isSyncErrorFixableByUserAction {
  SyncSetupService::SyncServiceState state =
      _syncSetupService->GetSyncServiceState();
  return state == SyncSetupService::kSyncServiceNeedsPassphrase ||
         state == SyncSetupService::kSyncServiceSignInNeedsUpdate ||
         state == SyncSetupService::kSyncServiceUnrecoverableError;
}

- (int)titleIdForSyncableDataType:(SyncSetupService::SyncableDatatype)datatype {
  switch (datatype) {
    case SyncSetupService::kSyncBookmarks:
      return IDS_SYNC_DATATYPE_BOOKMARKS;
    case SyncSetupService::kSyncOmniboxHistory:
      return IDS_SYNC_DATATYPE_TYPED_URLS;
    case SyncSetupService::kSyncPasswords:
      return IDS_SYNC_DATATYPE_PASSWORDS;
    case SyncSetupService::kSyncOpenTabs:
      return IDS_SYNC_DATATYPE_TABS;
    case SyncSetupService::kSyncAutofill:
      return IDS_SYNC_DATATYPE_AUTOFILL;
    case SyncSetupService::kSyncPreferences:
      return IDS_SYNC_DATATYPE_PREFERENCES;
    case SyncSetupService::kSyncReadingList:
      return IDS_SYNC_DATATYPE_READING_LIST;
    case SyncSetupService::kNumberOfSyncableDatatypes:
      NOTREACHED();
  }
  return 0;
}

- (BOOL)shouldEncryptionItemBeEnabled {
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(_browserState);
  return (syncService->IsEngineInitialized() &&
          _syncSetupService->IsSyncEnabled() &&
          ![self shouldDisableSettingsOnSyncError]);
}

- (BOOL)shouldSyncEverythingItemBeEnabled {
  return (_syncSetupService->IsSyncEnabled() &&
          ![self shouldDisableSettingsOnSyncError]);
}

- (BOOL)shouldSyncableItemsBeEnabled {
  return (!_syncSetupService->IsSyncingAllDataTypes() &&
          _syncSetupService->IsSyncEnabled() &&
          ![self shouldDisableSettingsOnSyncError]);
}

- (BOOL)isAutofillWalletImportItemEnabled {
  syncer::ModelType autofillModelType =
      _syncSetupService->GetModelType(SyncSetupService::kSyncAutofill);
  BOOL isAutofillOn = _syncSetupService->IsDataTypePreferred(autofillModelType);
  return isAutofillOn && [self shouldSyncableItemsBeEnabled];
}

- (BOOL)isAutofillWalletImportOn {
  return autofill::prefs::IsPaymentsIntegrationEnabled(
      _browserState->GetPrefs());
}

- (void)setAutofillWalletImportOn:(BOOL)on {
  autofill::prefs::SetPaymentsIntegrationEnabled(_browserState->GetPrefs(), on);
}

- (NSInteger)tagForIndexPath:(NSIndexPath*)indexPath {
  DCHECK(indexPath.section ==
         [self.tableViewModel
             sectionForSectionIdentifier:SectionIdentifierSyncServices]);
  NSInteger index = [self.tableViewModel indexInItemTypeForIndexPath:indexPath];
  return index + kTagShift;
}

- (NSIndexPath*)indexPathForTag:(NSInteger)shiftedTag {
  NSInteger unshiftedTag = shiftedTag - kTagShift;
  return [self.tableViewModel indexPathForItemType:ItemTypeSyncableDataType
                                 sectionIdentifier:SectionIdentifierSyncServices
                                           atIndex:unshiftedTag];
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_ignoreSyncStateChanges || _authenticationOperationInProgress) {
    return;
  }
  [self updateTableView];
}

#pragma mark signin::IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  if (_authenticationOperationInProgress) {
    return;
  }
  if (![self popViewIfSignedOut]) {
    // Only reload the view if it wasn't popped.
    [self reloadData];
  }
}

#pragma mark SettingsControllerProtocol callbacks

- (void)settingsWillBeDismissed {
  [self stopBrowserStateServiceObservers];
  [_authenticationFlow cancelAndDismiss];
}

#pragma mark - ChromeIdentityServiceObserver

- (void)profileUpdate:(ChromeIdentity*)identity {
  TableViewAccountItem* item = base::mac::ObjCCastStrict<TableViewAccountItem>(
      [_identityMap objectForKey:identity.gaiaID]);
  if (!item) {
    // Ignoring unknown identity.
    return;
  }
  [self updateAccountItem:item withIdentity:identity];
  [self reconfigureCellsForItems:@[ item ]];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
