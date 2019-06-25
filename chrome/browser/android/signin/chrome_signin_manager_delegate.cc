// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/chrome_signin_manager_delegate.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ChromeSigninManagerDelegate_jni.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "content/public/browser/storage_partition.h"
#include "services/identity/public/cpp/identity_manager.h"

using base::android::JavaParamRef;

namespace {

// Returns whether the user is a managed user or not.
bool ShouldLoadPolicyForUser(const std::string& username) {
  return !policy::BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

}  // namespace

ChromeSigninManagerDelegate::ChromeSigninManagerDelegate(JNIEnv* env,
                                                         jobject obj)
    : profile_(ProfileManager::GetActiveUserProfile()),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)),
      user_cloud_policy_manager_(profile_->GetUserCloudPolicyManager()),
      user_policy_signin_service_(
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_)),
      java_ref_(env, obj),
      weak_factory_(this) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(user_cloud_policy_manager_);
  DCHECK(user_policy_signin_service_);
}

ChromeSigninManagerDelegate::~ChromeSigninManagerDelegate() {}

void ChromeSigninManagerDelegate::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

void ChromeSigninManagerDelegate::StopApplyingCloudPolicy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  user_policy_signin_service_->ShutdownUserCloudPolicyManager();
}

void ChromeSigninManagerDelegate::RegisterPolicyWithAccount(
    const CoreAccountInfo& account,
    RegisterPolicyWithAccountCallback callback) {
  if (!ShouldLoadPolicyForUser(account.email)) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  user_policy_signin_service_->RegisterForPolicyWithAccountId(
      account.email, account.account_id,
      base::AdaptCallbackForRepeating(base::BindOnce(
          [](RegisterPolicyWithAccountCallback callback,
             const std::string& dm_token, const std::string& client_id) {
            base::Optional<ManagementCredentials> credentials;
            if (!dm_token.empty()) {
              credentials.emplace(dm_token, client_id);
            }
            std::move(callback).Run(credentials);
          },
          std::move(callback))));
}

void ChromeSigninManagerDelegate::FetchAndApplyCloudPolicy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_username,
    const base::android::JavaParamRef<jobject>& j_callback) {
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);
  DCHECK(!username.empty());
  // TODO(bsazonov): Remove after migrating the sign-in flow to CoreAccountId.
  // ExtractDomainName Dchecks that username is a valid email, in practice
  // this checks that @ is present and is not the last character.
  gaia::ExtractDomainName(username);
  CoreAccountInfo account =
      identity_manager_
          ->FindAccountInfoForAccountWithRefreshTokenByEmailAddress(username)
          .value();

  auto callback =
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback));

  RegisterPolicyWithAccount(
      account,
      base::BindOnce(&ChromeSigninManagerDelegate::OnPolicyRegisterDone,
                     weak_factory_.GetWeakPtr(), account, std::move(callback)));
}

void ChromeSigninManagerDelegate::OnPolicyRegisterDone(
    const CoreAccountInfo& account,
    base::OnceCallback<void()> policy_callback,
    const base::Optional<ManagementCredentials>& credentials) {
  if (credentials) {
    FetchPolicyBeforeSignIn(account, std::move(policy_callback),
                            credentials.value());
  } else {
    // User's account does not have a policy to fetch.
    std::move(policy_callback).Run();
  }
}

void ChromeSigninManagerDelegate::FetchPolicyBeforeSignIn(
    const CoreAccountInfo& account,
    base::OnceCallback<void()> policy_callback,
    const ManagementCredentials& credentials) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();
  user_policy_signin_service_->FetchPolicyForSignedInUser(
      AccountIdFromAccountInfo(account), credentials.dm_token,
      credentials.client_id, url_loader_factory,
      base::AdaptCallbackForRepeating(
          base::BindOnce([](base::OnceCallback<void()> callback,
                            bool success) { std::move(callback).Run(); },
                         std::move(policy_callback))));
}

void ChromeSigninManagerDelegate::IsAccountManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_username,
    const JavaParamRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(env, j_callback);
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);

  base::Optional<CoreAccountInfo> account =
      identity_manager_
          ->FindAccountInfoForAccountWithRefreshTokenByEmailAddress(username);

  RegisterPolicyWithAccount(
      account.value_or(CoreAccountInfo{}),
      base::BindOnce(
          [](base::android::ScopedJavaGlobalRef<jobject> callback,
             const base::Optional<ManagementCredentials>& credentials) {
            base::android::RunBooleanCallbackAndroid(callback,
                                                     credentials.has_value());
          },
          callback));
}

base::android::ScopedJavaLocalRef<jstring>
ChromeSigninManagerDelegate::GetManagementDomain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  base::android::ScopedJavaLocalRef<jstring> domain;

  policy::CloudPolicyStore* store = user_cloud_policy_manager_->core()->store();

  if (store && store->is_managed() && store->policy()->has_username()) {
    domain.Reset(base::android::ConvertUTF8ToJavaString(
        env, gaia::ExtractDomainName(store->policy()->username())));
  }

  return domain;
}

// instantiates ChromeSigninManagerDelegate
static jlong JNI_ChromeSigninManagerDelegate_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ChromeSigninManagerDelegate* chrome_signin_manager_delegate =
      new ChromeSigninManagerDelegate(env, obj);
  return reinterpret_cast<intptr_t>(chrome_signin_manager_delegate);
}
