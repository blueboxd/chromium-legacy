// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

// Android wrapper of Chrome's C++ identity management code which provides
// access from the Java layer. Note that on Android, there's only a single
// profile, and therefore a single instance of this wrapper. The name of the
// Java class is SigninManager. This class should only be accessed from the UI
// thread.
//
// This class implements parts of the sign-in flow, to make sure that policy
// is available before sign-in completes.
class SigninManagerAndroid : public identity::IdentityManager::Observer {
 public:
  SigninManagerAndroid(Profile* profile,
                       identity::IdentityManager* identity_manager);

  ~SigninManagerAndroid() override;

  void Shutdown();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Indicates that the user has made the choice to sign-in. |username|
  // contains the email address of the account to use as primary.
  void OnSignInCompleted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& username);

  void SignOut(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jint signoutReason);

  void LogInSignedInUser(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void ClearLastSignedInUser(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  jboolean IsSigninAllowedByPolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jboolean IsForceSigninEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jboolean IsSignedInOnNative(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

 private:
  void OnSigninAllowedPrefChanged();

  Profile* profile_;

  identity::IdentityManager* identity_manager_;

  // Java-side SigninManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_;

  PrefChangeRegistrar pref_change_registrar_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
