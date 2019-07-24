// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_CHROME_SIGNIN_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_CHROME_SIGNIN_MANAGER_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/signin/signin_manager_delegate.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace signin {
class IdentityManager;
}

namespace policy {
class UserCloudPolicyManager;
class UserPolicySigninService;
}  // namespace policy

class Profile;

// This class provide ChromeSigninManagerDelegate.java access to the native
// dependencies.
class ChromeSigninManagerDelegate : public SigninManagerDelegate {
 public:
  ChromeSigninManagerDelegate();

  ~ChromeSigninManagerDelegate() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;

  // Registers a CloudPolicyClient for fetching policy for a user and fetches
  // the policy if necessary.
  void FetchAndApplyCloudPolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& username,
      const base::android::JavaParamRef<jobject>& j_callback);

  void StopApplyingCloudPolicy(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  void IsAccountManaged(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jstring>& j_username,
                        const base::android::JavaParamRef<jobject>& j_callback);

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

 private:
  friend class ChromeSigninManagerDelegateTest;
  FRIEND_TEST_ALL_PREFIXES(ChromeSigninManagerDelegateTest,
                           DeleteGoogleServiceWorkerCaches);

  struct ManagementCredentials {
    ManagementCredentials(const std::string& dm_token,
                          const std::string& client_id)
        : dm_token(dm_token), client_id(client_id) {}
    const std::string dm_token;
    const std::string client_id;
  };

  using RegisterPolicyWithAccountCallback = base::OnceCallback<void(
      const base::Optional<ManagementCredentials>& credentials)>;

  ChromeSigninManagerDelegate(const ChromeSigninManagerDelegate&) = delete;

  ChromeSigninManagerDelegate& operator=(const ChromeSigninManagerDelegate&) =
      delete;

  // If required registers for policy with given account. callback will be
  // called with credentials if the account is managed.
  void RegisterPolicyWithAccount(const CoreAccountInfo& account,
                                 RegisterPolicyWithAccountCallback callback);

  void OnPolicyRegisterDone(
      const CoreAccountInfo& account_id,
      base::OnceCallback<void()> policy_callback,
      const base::Optional<ManagementCredentials>& credentials);

  void FetchPolicyBeforeSignIn(const CoreAccountInfo& account_id,
                               base::OnceCallback<void()> policy_callback,
                               const ManagementCredentials& credentials);

  static void WipeData(Profile* profile,
                       bool all_data,
                       base::OnceClosure callback);

  Profile* const profile_ = nullptr;

  signin::IdentityManager* const identity_manager_ = nullptr;
  policy::UserCloudPolicyManager* const user_cloud_policy_manager_ = nullptr;
  policy::UserPolicySigninService* const user_policy_signin_service_ = nullptr;

  // Java-side ChromeSigninManagerDelegate object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_delegate_;

  base::WeakPtrFactory<ChromeSigninManagerDelegate> weak_factory_;
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_CHROME_SIGNIN_MANAGER_DELEGATE_H_
