// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_manager_android.h"

#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/android/chrome_jni_headers/SigninManager_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/primary_account_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

using base::android::JavaParamRef;

namespace {

// Clears the information about the last signed-in user from |profile|.
void ClearLastSignedInUserForProfile(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
}

// A BrowsingDataRemover::Observer that clears Profile data and then invokes
// a callback and deletes itself. It can be configured to delete all data
// (for enterprise users) or only Google's service workers (for all users).
class ProfileDataRemover : public content::BrowsingDataRemover::Observer {
 public:
  ProfileDataRemover(Profile* profile,
                     bool all_data,
                     base::OnceClosure callback)
      : profile_(profile),
        all_data_(all_data),
        callback_(std::move(callback)),
        origin_runner_(base::ThreadTaskRunnerHandle::Get()),
        remover_(content::BrowserContext::GetBrowsingDataRemover(profile)) {
    remover_->AddObserver(this);

    if (all_data) {
      remover_->RemoveAndReply(
          base::Time(), base::Time::Max(),
          ChromeBrowsingDataRemoverDelegate::ALL_DATA_TYPES,
          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES, this);
    } else {
      std::unique_ptr<content::BrowsingDataFilterBuilder> google_tld_filter =
          content::BrowsingDataFilterBuilder::Create(
              content::BrowsingDataFilterBuilder::WHITELIST);

      // TODO(msramek): BrowsingDataFilterBuilder was not designed for
      // large filters. Optimize it.
      for (const std::string& domain :
           google_util::GetGoogleRegistrableDomains()) {
        google_tld_filter->AddRegisterableDomain(domain);
      }

      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(),
          content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE,
          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES,
          std::move(google_tld_filter), this);
    }
  }

  ~ProfileDataRemover() override {}

  void OnBrowsingDataRemoverDone() override {
    remover_->RemoveObserver(this);

    if (all_data_) {
      // All the Profile data has been wiped. Clear the last signed in username
      // as well, so that the next signin doesn't trigger the account
      // change dialog.
      ClearLastSignedInUserForProfile(profile_);
    }

    origin_runner_->PostTask(FROM_HERE, std::move(callback_));
    origin_runner_->DeleteSoon(FROM_HERE, this);
  }

 private:
  Profile* profile_;
  bool all_data_;
  base::OnceClosure callback_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_runner_;
  content::BrowsingDataRemover* remover_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDataRemover);
};

}  // namespace

SigninManagerAndroid::SigninManagerAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL),
      weak_factory_(this) {
  java_signin_manager_.Reset(env, obj);
  profile_ = ProfileManager::GetActiveUserProfile();
  DCHECK(profile_);
  IdentityManagerFactory::GetForProfile(profile_)->AddObserver(this);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&SigninManagerAndroid::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));
}

SigninManagerAndroid::~SigninManagerAndroid() {
  IdentityManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
  // TODO(crbug.com/963408) Call SigninManager.java Destroy once ownership is
  // reversed.
}

void SigninManagerAndroid::OnSignInCompleted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& username) {
  DVLOG(1) << "SigninManagerAndroid::OnSignInCompleted";

  // TODO(crbug.com/889902): Migrate to IdentityManager once there's an
  // API mapping for SigninManager::SignIn().
  IdentityManagerFactory::GetForProfile(profile_)
      ->GetPrimaryAccountManager()
      ->SignIn(base::android::ConvertJavaStringToUTF8(env, username));
}

void SigninManagerAndroid::SignOut(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jint signoutReason) {
  auto* account_mutator = IdentityManagerFactory::GetForProfile(profile_)
                              ->GetPrimaryAccountMutator();

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

void SigninManagerAndroid::WipeProfileData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback) {
  WipeData(
      profile_, true /* all data */,
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void SigninManagerAndroid::WipeGoogleServiceWorkerCaches(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback) {
  WipeData(
      profile_, false /* only Google service worker caches */,
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void SigninManagerAndroid::ClearLastSignedInUser(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ClearLastSignedInUserForProfile(profile_);
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  // With the account consistency enabled let the account Reconcilor handles
  // everything.
  // TODO(https://crbug.com/930094): Determine the right long-term flow here.
  identity_manager->LegacyReloadAccountsFromSystem();
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
  return IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount();
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

// static
void SigninManagerAndroid::WipeData(Profile* profile,
                                    bool all_data,
                                    base::OnceClosure callback) {
  // The ProfileDataRemover deletes itself once done.
  new ProfileDataRemover(profile, all_data, std::move(callback));
}

static jlong JNI_SigninManager_Init(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  SigninManagerAndroid* signin_manager_android =
      new SigninManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(signin_manager_android);
}

base::android::ScopedJavaLocalRef<jstring> JNI_SigninManager_ExtractDomainName(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_email) {
  std::string email = base::android::ConvertJavaStringToUTF8(env, j_email);
  std::string domain = gaia::ExtractDomainName(email);
  return base::android::ConvertUTF8ToJavaString(env, domain);
}
