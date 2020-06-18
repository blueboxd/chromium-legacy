// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"

#include <string>

#include "components/content_settings/core/common/features.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/ntp/incognito_view.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_commands.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/common/ui/colors/dynamic_color_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoViewController ()

// The scrollview containing the actual views.
@property(nonatomic, strong) IncognitoView* incognitoView;

@property(nonatomic, assign) CookiesSettingType cookiesSettingSelected;

@end

@implementation IncognitoViewController {
  // The UrlLoadingService associated with this view.
  UrlLoadingBrowserAgent* _URLLoader;  // weak
}

- (instancetype)initWithUrlLoader:(UrlLoadingBrowserAgent*)URLLoader {
  self = [super init];
  if (self) {
    _URLLoader = URLLoader;
  }
  return self;
}

- (void)viewDidLoad {
  if (@available(iOS 13, *)) {
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }

  self.incognitoView = [[IncognitoView alloc] initWithFrame:self.view.bounds
                                                  URLLoader:_URLLoader];
  [self.incognitoView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleWidth];
  UIColor* backgroundColor =
      color::DarkModeDynamicColor([UIColor colorNamed:kBackgroundColor], true,
                                  [UIColor colorNamed:kBackgroundDarkColor]);
  self.incognitoView.backgroundColor = backgroundColor;
  [self.view addSubview:self.incognitoView];

  if (self.incognitoView.cookiesBlockedSwitch) {
    [self.incognitoView.cookiesBlockedSwitch
               addTarget:self
                  action:@selector(onCookieSwitchToggled:)
        forControlEvents:UIControlEventValueChanged];

    // The mediator may have updated the cookies switch before before it's
    // returned.
    [self cookiesSettingsOptionSelected:self.cookiesSettingSelected];
  } else {
    // cookiesBlockedSwitch must exists if kImprovedCookieControls
    // feature is enabled.
    DCHECK(!base::FeatureList::IsEnabled(
        content_settings::kImprovedCookieControls));
  }
}

- (void)dealloc {
  [_incognitoView setDelegate:nil];
}

#pragma mark - Private

// Called when the cookies switch is toogled.
- (void)onCookieSwitchToggled:(UISwitch*)cookiesBlockedSwitch {
  [self.handler selectedCookiesSettingType:
                    self.incognitoView.cookiesBlockedSwitch.isOn
                        ? SettingTypeBlockThirdPartyCookiesIncognito
                        : SettingTypeAllowCookies];
}

#pragma mark - PrivacyCookiesConsumer

- (void)cookiesSettingsOptionSelected:(CookiesSettingType)settingType {
  self.cookiesSettingSelected = settingType;
  switch (settingType) {
    case SettingTypeBlockThirdPartyCookiesIncognito:
      self.incognitoView.cookiesBlockedSwitch.enabled = YES;
      [self.incognitoView.cookiesBlockedSwitch setOn:YES animated:YES];
      break;
    case SettingTypeBlockThirdPartyCookies:
      self.incognitoView.cookiesBlockedSwitch.enabled = NO;
      [self.incognitoView.cookiesBlockedSwitch setOn:YES animated:YES];
      break;
    case SettingTypeBlockAllCookies:
      self.incognitoView.cookiesBlockedSwitch.enabled = NO;
      [self.incognitoView.cookiesBlockedSwitch setOn:YES animated:YES];
      break;
    case SettingTypeAllowCookies:
      self.incognitoView.cookiesBlockedSwitch.enabled = YES;
      [self.incognitoView.cookiesBlockedSwitch setOn:NO animated:YES];
      break;
  }
}

@end
