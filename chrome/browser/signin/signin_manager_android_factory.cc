// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_android_factory.h"

#include "chrome/browser/android/signin/chrome_signin_manager_delegate.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SigninManagerAndroidFactory::SigninManagerAndroidFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninManagerAndroid",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninManagerAndroidFactory::~SigninManagerAndroidFactory() {}

// static
base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroidFactory::GetJavaObjectForProfile(Profile* profile) {
  return static_cast<SigninManagerAndroid*>(
             GetInstance()->GetServiceForBrowserContext(profile, true))
      ->GetJavaObject();
}

// static
SigninManagerAndroidFactory* SigninManagerAndroidFactory::GetInstance() {
  static base::NoDestructor<SigninManagerAndroidFactory> instance;
  return instance.get();
}

KeyedService* SigninManagerAndroidFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* signin_client = ChromeSigninClientFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto signin_manager_delegate =
      std::make_unique<ChromeSigninManagerDelegate>();

  return new SigninManagerAndroid(
      signin_client, g_browser_process->local_state(), identity_manager,
      std::move(signin_manager_delegate));
}
