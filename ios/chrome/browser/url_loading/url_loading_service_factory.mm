// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/url_loading/url_loading_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/url_loading/test_url_loading_service.h"
#include "ios/chrome/browser/url_loading/url_loading_notifier_factory.h"
#include "ios/chrome/browser/url_loading/url_loading_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
UrlLoadingService* UrlLoadingServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<UrlLoadingService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
UrlLoadingServiceFactory* UrlLoadingServiceFactory::GetInstance() {
  static base::NoDestructor<UrlLoadingServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService> BuildTestUrlLoadingService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<TestUrlLoadingService>(
      UrlLoadingNotifierFactory::GetForBrowserState(browser_state));
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
UrlLoadingServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildTestUrlLoadingService);
}

UrlLoadingServiceFactory::UrlLoadingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "UrlLoadingService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(UrlLoadingNotifierFactory::GetInstance());
}

UrlLoadingServiceFactory::~UrlLoadingServiceFactory() {}

std::unique_ptr<KeyedService> UrlLoadingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<UrlLoadingService>(
      UrlLoadingNotifierFactory::GetForBrowserState(browser_state));
}

web::BrowserState* UrlLoadingServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
