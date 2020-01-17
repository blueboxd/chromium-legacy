// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_list_factory.h"

#include <memory>

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_list_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
BrowserList* BrowserListFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<BrowserList*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BrowserListFactory* BrowserListFactory::GetInstance() {
  static base::NoDestructor<BrowserListFactory> instance;
  return instance.get();
}

BrowserListFactory::BrowserListFactory()
    : BrowserStateKeyedServiceFactory(
          "BrowserList",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService> BrowserListFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<BrowserListImpl>();
}

web::BrowserState* BrowserListFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Incognito browser states use same service as regular browser states.
  return GetBrowserStateRedirectedInIncognito(context);
}
