// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/cookies_status_mediator.h"

#import "base/logging.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_status_consumer.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_status_description.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, CookiesSettingType) {
  SettingTypeAllowCookies,
  SettingTypeBlockThirdPartyCookiesIncognito,
  SettingTypeBlockThirdPartyCookies,
  SettingTypeBlockAllCookies,
};

}  // namespace

@interface CookiesStatusMediator () <BooleanObserver> {
  // The preference that decides when the cookie controls UI is enabled.
  IntegerPrefMember _prefsCookieControlsMode;
}

// The observable boolean that binds to the "Enable cookie controls" setting
// state.
@property(nonatomic, strong)
    ContentSettingBackedBoolean* prefsContentSettingCookieControl;

@end

@implementation CookiesStatusMediator

- (instancetype)initWithPrefService:(PrefService*)prefService
                        settingsMap:(HostContentSettingsMap*)settingsMap {
  self = [super init];
  if (self) {
    __weak CookiesStatusMediator* weakSelf = self;
    _prefsCookieControlsMode.Init(prefs::kCookieControlsMode, prefService,
                                  base::BindRepeating(^() {
                                    [weakSelf updateConsumer];
                                  }));

    _prefsContentSettingCookieControl = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:ContentSettingsType::COOKIES
                              inverted:NO];
    [_prefsContentSettingCookieControl setObserver:self];
  }
  return self;
}

#pragma mark - Public

- (CookiesStatusDescription*)cookiesDescription {
  CookiesStatusDescription* dataHolder =
      [[CookiesStatusDescription alloc] init];
  dataHolder.headerDescription = [self cookiesStatus];
  dataHolder.footerDescription = [self cookiesFooterDescription];
  return dataHolder;
}

#pragma mark - Private

// Updates consumer.
- (void)updateConsumer {
  [self.consumer cookiesOptionChangedToDescription:[self cookiesDescription]];
}

// Returns the status of Cookies according to preferences.
- (NSString*)cookiesStatus {
  if (![self.prefsContentSettingCookieControl value])
    return l10n_util::GetNSString(IDS_IOS_COOKIES_BLOCK_ALL);

  if (self.prefsContentSettingCookieControl.value &&
      _prefsCookieControlsMode.GetValue() ==
          static_cast<int>(
              content_settings::CookieControlsMode::kBlockThirdParty))
    return l10n_util::GetNSString(IDS_IOS_COOKIES_BLOCK_THIRD_PARTY);

  if (_prefsCookieControlsMode.GetValue() ==
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly))
    return l10n_util::GetNSString(IDS_IOS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO);

  return l10n_util::GetNSString(IDS_IOS_COOKIES_ALLOW_ALL);
}

// Returns the footer description of Cookies according to preferences.
- (NSString*)cookiesFooterDescription {
  if (_prefsCookieControlsMode.GetValue() ==
      static_cast<int>(content_settings::CookieControlsMode::kOff))
    return l10n_util::GetNSString(
        IDS_IOS_PAGE_INFO_COOKIES_SETTINGS_LINK_LABEL_ALLOW_ALL);
  return l10n_util::GetNSString(
      IDS_IOS_PAGE_INFO_COOKIES_SETTINGS_LINK_LABEL_BLOCK);
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateConsumer];
}

@end
