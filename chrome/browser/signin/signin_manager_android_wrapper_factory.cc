// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_android_wrapper_factory.h"

#include "chrome/browser/android/signin/chrome_signin_manager_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include "chrome/browser/browser_process.h"

SigninManagerAndroidWrapperFactory::SigninManagerAndroidWrapperFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninManagerAndroidWrapper",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
}

SigninManagerAndroidWrapperFactory::~SigninManagerAndroidWrapperFactory() {}

// static
base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroidWrapperFactory::GetJavaObjectForProfile(Profile* profile) {
  return static_cast<SigninManagerAndroidWrapper*>(
             GetInstance()->GetServiceForBrowserContext(profile, true))
      ->GetJavaObject();
}

// static
SigninManagerAndroidWrapperFactory*
SigninManagerAndroidWrapperFactory::GetInstance() {
  static base::NoDestructor<SigninManagerAndroidWrapperFactory> instance;
  return instance.get();
}

KeyedService* SigninManagerAndroidWrapperFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* signin_client = ChromeSigninClientFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto signin_manager_delegate =
      std::make_unique<ChromeSigninManagerDelegate>();

  return new SigninManagerAndroidWrapper(
      signin_client, g_browser_process->local_state(), identity_manager,
      std::move(signin_manager_delegate));
}
