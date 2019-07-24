// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_manager_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/android/chrome_jni_headers/SigninManager_jni.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "google_apis/gaia/gaia_auth_util.h"

using base::android::JavaParamRef;

namespace {
// Clears the information about the last signed-in user from |profile|.
void ClearLastSignedInUserForProfile(SigninClient* signin_client) {
  signin_client->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
  signin_client->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
}
}  // namespace

SigninManagerAndroid::SigninManagerAndroid(
    SigninClient* signin_client,
    PrefService* local_state_pref_service,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SigninManagerDelegate> signin_manager_delegate)
    : signin_client_(signin_client),
      identity_manager_(identity_manager),
      signin_manager_delegate_(std::move(signin_manager_delegate)) {
  DCHECK(signin_client_);
  DCHECK(local_state_pref_service);
  DCHECK(identity_manager_);
  DCHECK(signin_manager_delegate_);
  identity_manager_->AddObserver(this);

  signin_allowed_.Init(
      prefs::kSigninAllowed, signin_client_->GetPrefs(),
      base::Bind(&SigninManagerAndroid::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));

  force_browser_signin_.Init(prefs::kForceBrowserSignin,
                             local_state_pref_service);

  java_signin_manager_ = Java_SigninManager_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      signin_manager_delegate_->GetJavaObject(),
      identity_manager_->LegacyGetAccountTrackerServiceJavaObject());
}

base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_signin_manager_);
}

SigninManagerAndroid::~SigninManagerAndroid() {}

void SigninManagerAndroid::Shutdown() {
  identity_manager_->RemoveObserver(this);
  Java_SigninManager_destroy(base::android::AttachCurrentThread(),
                             java_signin_manager_);
  signin_manager_delegate_.reset();
}

void SigninManagerAndroid::OnSignInCompleted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& username) {
  DVLOG(1) << "SigninManagerAndroid::OnSignInCompleted";

  // TODO(crbug.com/889902): Migrate to IdentityManager once there's an
  // API mapping for SigninManager::SignIn().
  identity_manager_->GetPrimaryAccountManager()->SignIn(
      base::android::ConvertJavaStringToUTF8(env, username));
}

void SigninManagerAndroid::SignOut(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jint signoutReason) {
  auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);
  account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      static_cast<signin_metrics::ProfileSignout>(signoutReason),
      // Always use IGNORE_METRIC for the profile deletion argument. Chrome
      // Android has just a single-profile which is never deleted upon
      // sign-out.
      signin_metrics::SignoutDelete::IGNORE_METRIC);
}

void SigninManagerAndroid::ClearLastSignedInUser(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ClearLastSignedInUserForProfile(signin_client_);
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  // With the account consistency enabled let the account Reconcilor handles
  // everything.
  // TODO(https://crbug.com/930094): Determine the right long-term flow here.
  identity_manager_->LegacyReloadAccountsFromSystem();
}

bool SigninManagerAndroid::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

jboolean SigninManagerAndroid::IsSigninAllowedByPolicy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return IsSigninAllowed();
}

jboolean SigninManagerAndroid::IsForceSigninEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return force_browser_signin_.GetValue();
}

jboolean SigninManagerAndroid::IsSignedInOnNative(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return identity_manager_->HasPrimaryAccount();
}

void SigninManagerAndroid::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Java_SigninManager_onNativeSignOut(base::android::AttachCurrentThread(),
                                     java_signin_manager_);
}

void SigninManagerAndroid::OnSigninAllowedPrefChanged() const {
  Java_SigninManager_onSigninAllowedByPolicyChanged(
      base::android::AttachCurrentThread(), java_signin_manager_,
      IsSigninAllowed());
}

base::android::ScopedJavaLocalRef<jstring> JNI_SigninManager_ExtractDomainName(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_email) {
  std::string email = base::android::ConvertJavaStringToUTF8(env, j_email);
  std::string domain = gaia::ExtractDomainName(email);
  return base::android::ConvertUTF8ToJavaString(env, domain);
}

jboolean JNI_SigninManager_IsMobileIdentityConsistencyEnabled(JNIEnv* env) {
  return base::FeatureList::IsEnabled(signin::kMiceFeature);
}
