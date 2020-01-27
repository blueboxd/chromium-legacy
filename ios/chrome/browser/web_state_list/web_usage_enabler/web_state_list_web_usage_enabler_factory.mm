// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
WebStateListWebUsageEnabler*
WebStateListWebUsageEnablerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<WebStateListWebUsageEnabler*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebStateListWebUsageEnablerFactory*
WebStateListWebUsageEnablerFactory::GetInstance() {
  static base::NoDestructor<WebStateListWebUsageEnablerFactory> instance;
  return instance.get();
}

WebStateListWebUsageEnablerFactory::WebStateListWebUsageEnablerFactory()
    : BrowserStateKeyedServiceFactory(
          "WebStateListWebUsageEnabler",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebStateListWebUsageEnablerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<WebStateListWebUsageEnablerImpl>();
}

web::BrowserState* WebStateListWebUsageEnablerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
