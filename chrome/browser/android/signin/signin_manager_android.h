// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/identity/public/cpp/identity_manager.h"

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
  SigninManagerAndroid(JNIEnv* env, jobject obj);

  // Registers a CloudPolicyClient for fetching policy for a user and fetches
  // the policy if necessary.
  void FetchAndApplyCloudPolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& username);

  void AbortSignIn(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

  // Indicates that the user has made the choice to sign-in. |username|
  // contains the email address of the account to use as primary.
  void OnSignInCompleted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& username);

  void SignOut(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jint signoutReason);

  base::android::ScopedJavaLocalRef<jstring> GetManagementDomain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Delete all data for this profile.
  void WipeProfileData(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jobject>& j_callback);

  // Delete service worker caches for google.<eTLD>.
  void WipeGoogleServiceWorkerCaches(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_callback);

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

  void IsUserManaged(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& j_username,
                     const base::android::JavaParamRef<jobject>& j_callback);

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

 private:
  struct ManagementCredentials {
    ManagementCredentials(const std::string& dm_token,
                          const std::string& client_id)
        : dm_token(dm_token), client_id(client_id) {}
    const std::string dm_token;
    const std::string client_id;
  };

  using RegisterPolicyWithAccountCallback = base::OnceCallback<void(
      const base::Optional<ManagementCredentials>& credentials)>;

  friend class SigninManagerAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerAndroidTest,
                           DeleteGoogleServiceWorkerCaches);

  ~SigninManagerAndroid() override;

  // If required registers for policy with given account. callback will be
  // called with credentials if the account is managed.
  void RegisterPolicyWithAccount(const CoreAccountInfo& account,
                                 RegisterPolicyWithAccountCallback callback);

  void OnPolicyRegisterDone(
      const CoreAccountInfo& account_id,
      const base::Optional<ManagementCredentials>& credentials);

  void FetchPolicyBeforeSignIn(const CoreAccountInfo& account_id,
                               const ManagementCredentials& credentials);

  void OnPolicyFetchDone(bool success) const;

  void OnBrowsingDataRemoverDone();

  void OnSigninAllowedPrefChanged();

  static void WipeData(Profile* profile,
                       bool all_data,
                       base::OnceClosure callback);

  Profile* profile_;

  // Java-side SigninManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_;

  PrefChangeRegistrar pref_change_registrar_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SigninManagerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
