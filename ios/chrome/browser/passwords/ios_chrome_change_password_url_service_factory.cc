// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_change_password_url_service_factory.h"

#include <memory>
#include <utility>

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/password_manager/core/browser/change_password_url_service_impl.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
IOSChromeChangePasswordUrlServiceFactory*
IOSChromeChangePasswordUrlServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeChangePasswordUrlServiceFactory> instance;
  return instance.get();
}

// static
password_manager::ChangePasswordUrlService*
IOSChromeChangePasswordUrlServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<password_manager::ChangePasswordUrlService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeChangePasswordUrlServiceFactory::
    IOSChromeChangePasswordUrlServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ChangePasswordUrlService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeChangePasswordUrlServiceFactory::
    ~IOSChromeChangePasswordUrlServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSChromeChangePasswordUrlServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<password_manager::ChangePasswordUrlServiceImpl>(
      context->GetSharedURLLoaderFactory(), browser_state->GetPrefs());
}
