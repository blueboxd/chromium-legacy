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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/primary_account_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "google_apis/gaia/gaia_auth_util.h"

using base::android::JavaParamRef;

namespace {
// Clears the information about the last signed-in user from |profile|.
void ClearLastSignedInUserForProfile(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
}
}  // namespace

SigninManagerAndroid::SigninManagerAndroid(
    Profile* profile,
    identity::IdentityManager* identity_manager)
    : profile_(profile), identity_manager_(identity_manager) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  identity_manager_->AddObserver(this);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&SigninManagerAndroid::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));
  java_signin_manager_ = Java_SigninManager_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_signin_manager_);
}

SigninManagerAndroid::~SigninManagerAndroid() {
  Java_SigninManager_destroy(base::android::AttachCurrentThread(),
                             java_signin_manager_);
}

void SigninManagerAndroid::Shutdown() {
  identity_manager_->RemoveObserver(this);
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
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      static_cast<signin_metrics::ProfileSignout>(signoutReason),
      // Always use IGNORE_METRIC for the profile deletion argument. Chrome
      // Android has just a single-profile which is never deleted upon
      // sign-out.
      signin_metrics::SignoutDelete::IGNORE_METRIC);
}

void SigninManagerAndroid::ClearLastSignedInUser(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ClearLastSignedInUserForProfile(profile_);
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  // With the account consistency enabled let the account Reconcilor handles
  // everything.
  // TODO(https://crbug.com/930094): Determine the right long-term flow here.
  identity_manager_->LegacyReloadAccountsFromSystem();
}

jboolean SigninManagerAndroid::IsSigninAllowedByPolicy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return profile_->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
}

jboolean SigninManagerAndroid::IsForceSigninEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // prefs::kForceBrowserSignin is set in Local State, not in user prefs.
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(prefs::kForceBrowserSignin);
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

void SigninManagerAndroid::OnSigninAllowedPrefChanged() {
  Java_SigninManager_onSigninAllowedByPolicyChanged(
      base::android::AttachCurrentThread(), java_signin_manager_,
      profile_->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
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
