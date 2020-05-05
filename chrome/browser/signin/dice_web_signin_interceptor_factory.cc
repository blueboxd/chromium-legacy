// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
DiceWebSigninInterceptor* DiceWebSigninInterceptorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DiceWebSigninInterceptor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

//  static
DiceWebSigninInterceptorFactory*
DiceWebSigninInterceptorFactory::GetInstance() {
  return base::Singleton<DiceWebSigninInterceptorFactory>::get();
}

DiceWebSigninInterceptorFactory::DiceWebSigninInterceptorFactory()
    : BrowserContextKeyedServiceFactory(
          "DiceWebSigninInterceptor",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

DiceWebSigninInterceptorFactory::~DiceWebSigninInterceptorFactory() = default;

KeyedService* DiceWebSigninInterceptorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DiceWebSigninInterceptor(Profile::FromBrowserContext(context));
}
