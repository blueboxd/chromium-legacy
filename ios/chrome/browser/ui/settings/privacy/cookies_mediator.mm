// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/cookies_mediator.h"

#import "base/logging.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_consumer.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrivacyCookiesMediator () <BooleanObserver> {
  // The preference that decides when the cookie controls UI is enabled.
  IntegerPrefMember _prefsCookieControlsMode;
}

// The observable boolean that binds to the "Enable cookie controls" setting
// state.
@property(nonatomic, strong)
    ContentSettingBackedBoolean* prefsContentSettingCookieControl;

@end

@implementation PrivacyCookiesMediator

- (instancetype)initWithPrefService:(PrefService*)prefService
                        settingsMap:(HostContentSettingsMap*)settingsMap {
  self = [super init];
  if (self) {
    __weak PrivacyCookiesMediator* weakSelf = self;
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

- (void)setConsumer:(id<PrivacyCookiesConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;
  [self updateConsumer];
}

#pragma mark - PrivacyCookiesCommands

- (void)selectedCookiesSettingType:(CookiesSettingType)settingType {
  switch (settingType) {
    case SettingTypeAllowCookies:
      [self.prefsContentSettingCookieControl setValue:YES];
      _prefsCookieControlsMode.SetValue(
          static_cast<int>(content_settings::CookieControlsMode::kOff));
      break;

    case SettingTypeBlockThirdPartyCookiesIncognito:
      [self.prefsContentSettingCookieControl setValue:YES];
      _prefsCookieControlsMode.SetValue(static_cast<int>(
          content_settings::CookieControlsMode::kIncognitoOnly));
      break;

    case SettingTypeBlockThirdPartyCookies:
      [self.prefsContentSettingCookieControl setValue:YES];
      _prefsCookieControlsMode.SetValue(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty));
      break;

    case SettingTypeBlockAllCookies:
      [self.prefsContentSettingCookieControl setValue:NO];
      _prefsCookieControlsMode.SetValue(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty));
      break;
    default:
      NOTREACHED();
      break;
  }
  [self updateConsumer];
}

#pragma mark - Private

// Returns the cookiesSettingType according to preferences.
- (CookiesSettingType)cookiesSettingType {
  if (![self.prefsContentSettingCookieControl value])
    return SettingTypeBlockAllCookies;

  if (self.prefsContentSettingCookieControl.value &&
      _prefsCookieControlsMode.GetValue() ==
          static_cast<int>(
              content_settings::CookieControlsMode::kBlockThirdParty))
    return SettingTypeBlockThirdPartyCookies;

  if (_prefsCookieControlsMode.GetValue() ==
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly))
    return SettingTypeBlockThirdPartyCookiesIncognito;

  return SettingTypeAllowCookies;
}

- (void)updateConsumer {
  [self.consumer cookiesSettingsOptionSelected:[self cookiesSettingType]];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateConsumer];
}

@end
