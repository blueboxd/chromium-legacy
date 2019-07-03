// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_android_wrapper_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SigninManagerAndroidWrapperFactory::SigninManagerAndroidWrapperFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninManagerAndroidWrapper",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
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
  return new SigninManagerAndroidWrapper(
      profile, IdentityManagerFactory::GetForProfile(profile));
}
