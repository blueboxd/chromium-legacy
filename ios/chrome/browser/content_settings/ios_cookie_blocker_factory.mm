// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_settings/ios_cookie_blocker_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/common/features.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#import "ios/chrome/browser/content_settings/ios_cookie_blocker.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
IOSCookieBlocker* IOSCookieBlockerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<IOSCookieBlocker*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSCookieBlockerFactory* IOSCookieBlockerFactory::GetInstance() {
  static base::NoDestructor<IOSCookieBlockerFactory> instance;
  return instance.get();
}

IOSCookieBlockerFactory::IOSCookieBlockerFactory()
    : BrowserStateKeyedServiceFactory(
          "IOSCookieBlocker",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::CookieSettingsFactory::GetInstance());
}

IOSCookieBlockerFactory::~IOSCookieBlockerFactory() {}

web::BrowserState* IOSCookieBlockerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // The incognito browser state has its own CookieSettings map. Therefore, it
  // should get its own IOSCookieBlocker.
  return GetBrowserStateOwnInstanceInIncognito(context);
}

std::unique_ptr<KeyedService> IOSCookieBlockerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<IOSCookieBlocker>(
      browser_state,
      ios::CookieSettingsFactory::GetForBrowserState(browser_state));
}

bool IOSCookieBlockerFactory::ServiceIsCreatedWithBrowserState() const {
  return base::FeatureList::IsEnabled(
      content_settings::kImprovedCookieControls);
}
