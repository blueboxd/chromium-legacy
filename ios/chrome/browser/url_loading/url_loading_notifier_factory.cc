// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/url_loading/url_loading_notifier_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/url_loading/url_loading_notifier.h"

// static
UrlLoadingNotifier* UrlLoadingNotifierFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<UrlLoadingNotifier*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
UrlLoadingNotifierFactory* UrlLoadingNotifierFactory::GetInstance() {
  static base::NoDestructor<UrlLoadingNotifierFactory> instance;
  return instance.get();
}

UrlLoadingNotifierFactory::UrlLoadingNotifierFactory()
    : BrowserStateKeyedServiceFactory(
          "UrlLoadingNotifier",
          BrowserStateDependencyManager::GetInstance()) {}
UrlLoadingNotifierFactory::~UrlLoadingNotifierFactory() {}

std::unique_ptr<KeyedService>
UrlLoadingNotifierFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<UrlLoadingNotifier>();
}

web::BrowserState* UrlLoadingNotifierFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
