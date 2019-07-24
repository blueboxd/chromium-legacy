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
#include "chrome/browser/android/signin/signin_manager_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class SigninClient;

// Android wrapper of Chrome's C++ identity management code which provides
// access from the Java layer. Note that on Android, there's only a single
// profile, and therefore a single instance of this wrapper. The name of the
// Java class is SigninManager. This class should only be accessed from the UI
// thread.
//
// This class implements parts of the sign-in flow, to make sure that policy
// is available before sign-in completes.
class SigninManagerAndroid : public KeyedService,
                             public signin::IdentityManager::Observer {
 public:
  SigninManagerAndroid(
      SigninClient* signin_client,
      PrefService* local_state_prefs_service,
      signin::IdentityManager* identity_manager,
      std::unique_ptr<SigninManagerDelegate> signin_manager_delegate);

  ~SigninManagerAndroid() override;

  void Shutdown() override;

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
      const base::android::JavaParamRef<jobject>& obj) const;

  jboolean IsForceSigninEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jboolean IsSignedInOnNative(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

 private:
  void OnSigninAllowedPrefChanged() const;
  bool IsSigninAllowed() const;

  SigninClient* signin_client_;

  // Handler for prefs::kSigninAllowed set in user's profile.
  BooleanPrefMember signin_allowed_;

  // Handler for prefs::kForceBrowserSignin. This preference is set in Local
  // State, not in user prefs.
  BooleanPrefMember force_browser_signin_;

  signin::IdentityManager* identity_manager_;

  std::unique_ptr<SigninManagerDelegate> signin_manager_delegate_;

  // Java-side SigninManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
