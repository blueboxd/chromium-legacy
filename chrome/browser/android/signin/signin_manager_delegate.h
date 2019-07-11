// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_DELEGATE_H_

#include "base/android/scoped_java_ref.h"

// This class provides SigninManager.java access to dependencies on
// chrome/browser through its derivated class ChromeSigninManagerDelegate and
// the java counterparts SigninManagerDelegate.java and
// ChromeSigninManagerDelegate.java.
class SigninManagerDelegate {
 public:
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;

  SigninManagerDelegate() = default;

  virtual ~SigninManagerDelegate() = default;

  SigninManagerDelegate(const SigninManagerDelegate&) = delete;

  SigninManagerDelegate& operator=(const SigninManagerDelegate&) = delete;
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_DELEGATE_H_
