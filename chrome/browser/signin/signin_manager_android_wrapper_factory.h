// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_WRAPPER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_WRAPPER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/signin/signin_manager_android_wrapper.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

class SigninManagerAndroidWrapperFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObjectForProfile(
      Profile* profile);

  // Returns an instance of the SigninManagerAndroidWrapperFactory singleton.
  static SigninManagerAndroidWrapperFactory* GetInstance();

 private:
  friend class base::NoDestructor<SigninManagerAndroidWrapperFactory>;
  SigninManagerAndroidWrapperFactory();

  ~SigninManagerAndroidWrapperFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_WRAPPER_FACTORY_H_
