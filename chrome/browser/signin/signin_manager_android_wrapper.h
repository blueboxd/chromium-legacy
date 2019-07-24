// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_WRAPPER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_WRAPPER_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class SigninManagerAndroidWrapper : public KeyedService {
 public:
  // initializes the member signin_manager_android_ and keeps ownership.
  SigninManagerAndroidWrapper(
      SigninClient* signin_client,
      PrefService* local_state_prefs_service,
      signin::IdentityManager* identity_manager,
      std::unique_ptr<SigninManagerDelegate> signin_manager_delegate);

  ~SigninManagerAndroidWrapper() override;

  void Shutdown() override;

  // Returns the reference on SigninManager.java owned by
  // signin_manager_android_
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  SigninManagerAndroid signin_manager_android_;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_WRAPPER_H_
