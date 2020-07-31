// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"

#include "base/mac/foundation_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_multiline_detail_item.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_consumer.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_service_delegate.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;
using safe_browsing::kSafeBrowsingAvailableOnIOS;

namespace {

typedef NSArray<TableViewItem*>* ItemArray;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCheckTypes = kSectionIdentifierEnumZero,
  SectionIdentifierCheckStart,
};

typedef NS_ENUM(NSInteger, ItemType) {
  // CheckTypes section.
  UpdateItemType = kItemTypeEnumZero,
  PasswordItemType,
  SafeBrowsingItemType,
  // CheckStart section.
  CheckStartItemType,
};

// Enum with all possible states of the update check.
typedef NS_ENUM(NSInteger, UpdateCheckStates) {
  // When the user is up to date.
  UpdateCheckStateUpToDate,
  // When the check has not been run yet.
  UpdateCheckStateDefault,
  // When the user is out of date.
  UpdateCheckStateOutOfDate,
  // When the user is managed.
  UpdateCheckStateManaged,
};

// Enum with all possible states of the password check.
typedef NS_ENUM(NSInteger, PasswordCheckStates) {
  // When no compromised passwords were detected.
  PasswordCheckStateSafe,
  // When user has compromised passwords.
  PasswordCheckStateUnSafe,
  // When check has not been run yet.
  PasswordCheckStateDefault,
  // When password check is running.
  PasswordCheckStateRunning,
  // When user has no passwords and check can't be performed.
  PasswordCheckStateDisabled,
  // When password check failed due to network issues, quota limit or others.
  PasswordCheckStateError,
};

// Enum with all possible states of the Safe Browsing check.
typedef NS_ENUM(NSInteger, SafeBrowsingCheckStates) {
  // When check was not run yet.
  SafeBrowsingCheckStateDefault,
  // When Safe Browsing is managed by admin.
  SafeBrowsingCheckStateManged,
  // When the Safe Browsing check is running.
  SafeBrowsingCheckStateRunning,
  // When Safe Browsing is enabled.
  SafeBrowsingCheckStateSafe,
  // When Safe Browsing is disabled.
  SafeBrowsingCheckStateUnsafe,
};

// Enum with all possible states of the button to start the check.
typedef NS_ENUM(NSInteger, CheckStartStates) {
  // When the check is not running.
  CheckStartStateDefault,
  // When the check is running.
  CheckStartStateCancel,
};

}  // namespace

@interface SafetyCheckMediator () <BooleanObserver>

// SettingsCheckItem used to display the state of the Safe Browsing check.
@property(nonatomic, strong) SettingsCheckItem* safeBrowsingCheckItem;

// Current state of the Safe Browsing check.
@property(nonatomic, assign) SafeBrowsingCheckStates safeBrowsingCheckState;

// SettingsCheckItem used to display the state of the update check.
@property(nonatomic, strong) SettingsCheckItem* updateCheckItem;

// Current state of the update check.
@property(nonatomic, assign) UpdateCheckStates updateCheckState;

// SettingsCheckItem used to display the state of the password check.
@property(nonatomic, strong) SettingsCheckItem* passwordCheckItem;

// Current state of the password check.
@property(nonatomic, assign) PasswordCheckStates passwordCheckState;

// Row button to start the safety check.
@property(nonatomic, strong) SettingsMultilineDetailItem* startCheckItem;

// Current state of the start safety check row button.
@property(nonatomic, assign) CheckStartStates startCheckState;

// Preference value for the "Safe Browsing" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingPreference;

@end

@implementation SafetyCheckMediator

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    _safeBrowsingPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnabled];
    _safeBrowsingPreference.observer = self;

    _updateCheckState = UpdateCheckStateDefault;
    _updateCheckItem = [[SettingsCheckItem alloc] initWithType:UpdateItemType];

    _passwordCheckState = PasswordCheckStateDefault;
    _passwordCheckItem =
        [[SettingsCheckItem alloc] initWithType:PasswordItemType];

    _safeBrowsingCheckState = SafeBrowsingCheckStateDefault;
    _safeBrowsingCheckItem =
        [[SettingsCheckItem alloc] initWithType:SafeBrowsingItemType];

    _startCheckState = CheckStartStateDefault;
    _startCheckItem =
        [[SettingsMultilineDetailItem alloc] initWithType:CheckStartItemType];
  }
  return self;
}

#pragma mark - Private

// Loads SectionIdentifierCheckTypes section.
- (void)loadCheckTypesSection {
  NSMutableArray* items = [NSMutableArray array];

  self.updateCheckItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_TITLE);
  self.updateCheckItem.enabled = NO;
  [items addObject:self.updateCheckItem];

  self.passwordCheckItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_PASSWORDS_TITLE);
  self.passwordCheckItem.enabled = NO;
  [items addObject:self.passwordCheckItem];

  self.safeBrowsingCheckItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_TITLE);
  self.safeBrowsingCheckItem.enabled = NO;
  [items addObject:self.safeBrowsingCheckItem];

  [self.consumer setCheckItems:items];
}

// Loads SectionIdentifierCheckStart section.
- (void)loadCheckStartSection {
  self.startCheckItem.text = GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
  [self.consumer setCheckStartItem:self.startCheckItem];
}

#pragma mark - SafetyCheckServiceDelegate

- (void)didSelectItem:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    // TODO(crbug.com/1078782): Handle row taps.
    case UpdateItemType:
      break;
    case SafeBrowsingItemType:
      break;
    case PasswordItemType:
      break;
    case CheckStartItemType:
      break;
  }
}

#pragma mark - Public

// Update the consumer with the current state of the safety check.
// TODO(crbug.com/1078782): Have this handle more than the initial loading.
- (void)updateConsumerCheckState {
  [self loadCheckTypesSection];
  [self loadCheckStartSection];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  // TODO(crbug.com/1078782): Handle safe browsing state changes.
  return;
}

@end
