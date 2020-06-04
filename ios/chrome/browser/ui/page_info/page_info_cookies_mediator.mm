// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_cookies_mediator.h"

#import "base/logging.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/page_info/page_info_cookies_consumer.h"
#import "ios/chrome/browser/ui/page_info/page_info_cookies_description.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/web/public/web_state_observer.h"

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

@interface PageInfoCookiesMediator () <BooleanObserver,
                                       PageInfoCookiesDelegate> {
  // The preference that decides when the cookie controls UI is enabled.
  IntegerPrefMember _prefsCookieControlsMode;
}

// The observable boolean that binds to the "Enable cookie controls" setting
// state.
@property(nonatomic, strong)
    ContentSettingBackedBoolean* prefsContentSettingCookieControl;

@end

@implementation PageInfoCookiesMediator

// TODO(crbug.com/1038919): Use webState to retrieve Cookies information.
- (instancetype)initWithWebState:(web::WebState*)webState
                     prefService:(PrefService*)prefService
                     settingsMap:(HostContentSettingsMap*)settingsMap {
  self = [super init];
  if (self) {
    __weak PageInfoCookiesMediator* weakSelf = self;
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

- (PageInfoCookiesDescription*)cookiesDescription {
  PageInfoCookiesDescription* dataHolder =
      [[PageInfoCookiesDescription alloc] init];

  dataHolder.blockThirdPartyCookies =
      [self cookiesSettingType] == SettingTypeBlockThirdPartyCookies;
  // TODO(crbug.com/1038919): Implement this,the value is a placeholder.
  dataHolder.blockedCookies = 7;
  // TODO(crbug.com/1038919): Implement this,the value is a placeholder.
  dataHolder.cookiesInUse = 44;
  return dataHolder;
}

#pragma mark - Private

// Updates consumer.
- (void)updateConsumer {
  [self.consumer cookiesSwitchChanged:[self cookiesSettingType] ==
                                      SettingTypeBlockThirdPartyCookies];
}

// Returns the cookiesSettingType according to preferences.
- (CookiesSettingType)cookiesSettingType {
  if (!self.prefsContentSettingCookieControl.value)
    return SettingTypeBlockAllCookies;

  const content_settings::CookieControlsMode mode =
      static_cast<content_settings::CookieControlsMode>(
          _prefsCookieControlsMode.GetValue());

  switch (mode) {
    case content_settings::CookieControlsMode::kBlockThirdParty:
      return SettingTypeBlockThirdPartyCookies;
    case content_settings::CookieControlsMode::kIncognitoOnly:
      return SettingTypeBlockThirdPartyCookiesIncognito;
    default:
      return SettingTypeAllowCookies;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateConsumer];
}

#pragma mark - PageInfoCookiesDelegate

- (void)blockThirdPartyCookies:(BOOL)blocked {
  if (blocked) {
    [self.prefsContentSettingCookieControl setValue:YES];
    _prefsCookieControlsMode.SetValue(static_cast<int>(
        content_settings::CookieControlsMode::kBlockThirdParty));
  } else {
    [self.prefsContentSettingCookieControl setValue:YES];
    _prefsCookieControlsMode.SetValue(
        static_cast<int>(content_settings::CookieControlsMode::kOff));
  }
  [self updateConsumer];
}

@end
