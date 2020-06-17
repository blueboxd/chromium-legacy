// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <string>

#include "base/check.h"
#include "base/i18n/case_conversion.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"

DiceWebSigninInterceptor::DiceWebSigninInterceptor(
    Profile* profile,
    std::unique_ptr<Delegate> delegate)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      delegate_(std::move(delegate)) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(delegate_);
}

DiceWebSigninInterceptor::~DiceWebSigninInterceptor() = default;

void DiceWebSigninInterceptor::MaybeInterceptWebSignin(
    content::WebContents* web_contents,
    CoreAccountId account_id,
    bool is_new_account,
    bool is_sync_signin) {
  if (!base::FeatureList::IsEnabled(kDiceWebSigninInterceptionFeature))
    return;

  // Do not intercept signins from the Sync startup flow. Note: |is_sync_signin|
  // is an approximation, and in rare cases it may be true when in fact the
  // signin was not a sync signin. In this case the interception is missed.
  if (is_sync_signin)
    return;

  if (is_interception_in_progress_)
    return;  // Multiple concurrent interceptions are not supported.
  if (!is_new_account)
    return;  // Do not intercept reauth.

  account_id_ = account_id;
  is_interception_in_progress_ = true;
  Observe(web_contents);

  base::Optional<AccountInfo> account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id_);
  DCHECK(account_info) << "Intercepting unknown account.";

  if (ShouldShowProfileSwitchBubble(*account_info,
                                    &g_browser_process->profile_manager()
                                         ->GetProfileAttributesStorage())) {
    delegate_->ShowSigninInterceptionBubble(
        SigninInterceptionType::kProfileSwitch, web_contents, *account_info,
        base::BindOnce(&DiceWebSigninInterceptor::OnProfileSwitchChoice,
                       base::Unretained(this)));

    return;
  }

  if (identity_manager_->GetAccountsWithRefreshTokens().size() <= 1u) {
    // Enterprise and multi-user bubbles are only shown if there are multiple
    // accounts.
    Reset();
    return;
  }

  if (account_info->IsValid()) {
    OnExtendedAccountInfoUpdated(*account_info);
  } else {
    on_account_info_update_timeout_.Reset(base::BindOnce(
        &DiceWebSigninInterceptor::Reset, base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, on_account_info_update_timeout_.callback(),
        base::TimeDelta::FromSeconds(5));
    account_info_update_observer_.Add(identity_manager_);
  }
}

void DiceWebSigninInterceptor::CreateBrowserAfterSigninInterception(
    CoreAccountId account_id,
    content::WebContents* intercepted_contents) {
  DCHECK(!session_startup_helper_);
  session_startup_helper_ =
      std::make_unique<DiceInterceptedSessionStartupHelper>(
          profile_, account_id, intercepted_contents);
  session_startup_helper_->Startup(
      base::Bind(&DiceWebSigninInterceptor::DeleteSessionStartupHelper,
                 base::Unretained(this)));
}

void DiceWebSigninInterceptor::Shutdown() {
  Reset();
}

void DiceWebSigninInterceptor::Reset() {
  Observe(/*web_contents=*/nullptr);
  account_info_update_observer_.RemoveAll();
  on_account_info_update_timeout_.Cancel();
  is_interception_in_progress_ = false;
  account_id_ = CoreAccountId();
  dice_signed_in_profile_creator_.reset();
}

bool DiceWebSigninInterceptor::ShouldShowProfileSwitchBubble(
    const CoreAccountInfo& intercepted_account_info,
    ProfileAttributesStorage* profile_attribute_storage) {
  // Check if there is already an existing profile with this account.
  base::FilePath profile_path = profile_->GetPath();
  for (const auto* entry :
       profile_attribute_storage->GetAllProfilesAttributes()) {
    if (entry->GetPath() == profile_path)
      continue;
    if (entry->GetGAIAId() == intercepted_account_info.gaia)
      return true;
  }
  return false;
}

bool DiceWebSigninInterceptor::ShouldShowEnterpriseBubble(
    const AccountInfo& intercepted_account_info) {
  DCHECK(intercepted_account_info.IsValid());
  // Check if the intercepted account or the primary account is managed.
  CoreAccountInfo primary_core_account_info =
      identity_manager_->GetPrimaryAccountInfo(
          signin::ConsentLevel::kNotRequired);

  if (primary_core_account_info.IsEmpty() ||
      primary_core_account_info.account_id ==
          intercepted_account_info.account_id) {
    return false;
  }

  if (intercepted_account_info.hosted_domain != kNoHostedDomainFound)
    return true;

  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          primary_core_account_info);
  if (!primary_account_info || !primary_account_info->IsValid())
    return false;

  return primary_account_info->hosted_domain != kNoHostedDomainFound;
}

bool DiceWebSigninInterceptor::ShouldShowMultiUserBubble(
    const AccountInfo& intercepted_account_info) {
  DCHECK(intercepted_account_info.IsValid());
  if (identity_manager_->GetAccountsWithRefreshTokens().size() <= 1u)
    return false;
  // Check if the account has the same name as another account in the profile.
  for (const auto& account_info :
       identity_manager_->GetExtendedAccountInfoForAccountsWithRefreshToken()) {
    if (account_info.account_id == intercepted_account_info.account_id)
      continue;
    // Case-insensitve comparison supporting non-ASCII characters.
    if (base::i18n::FoldCase(base::UTF8ToUTF16(account_info.given_name)) ==
        base::i18n::FoldCase(
            base::UTF8ToUTF16(intercepted_account_info.given_name))) {
      return false;
    }
  }
  return true;
}

void DiceWebSigninInterceptor::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id != account_id_)
    return;
  if (!info.IsValid())
    return;

  account_info_update_observer_.RemoveAll();
  on_account_info_update_timeout_.Cancel();

  base::Optional<SigninInterceptionType> interception_type;

  if (ShouldShowEnterpriseBubble(info))
    interception_type = SigninInterceptionType::kEnterprise;
  else if (ShouldShowMultiUserBubble(info))
    interception_type = SigninInterceptionType::kMultiUser;

  if (!interception_type) {
    // Signin should not be intercepted.
    Reset();
    return;
  }

  delegate_->ShowSigninInterceptionBubble(
      *interception_type, web_contents(), info,
      base::BindOnce(&DiceWebSigninInterceptor::OnProfileCreationChoice,
                     base::Unretained(this)));
}

void DiceWebSigninInterceptor::OnProfileCreationChoice(bool create) {
  if (!create) {
    Reset();
    return;
  }

  DCHECK(!dice_signed_in_profile_creator_);
  // Unretained is fine because the profile creator is owned by this.
  dice_signed_in_profile_creator_ =
      std::make_unique<DiceSignedInProfileCreator>(
          profile_, account_id_,
          base::BindOnce(&DiceWebSigninInterceptor::OnNewSignedInProfileCreated,
                         base::Unretained(this)));
}

void DiceWebSigninInterceptor::OnProfileSwitchChoice(bool switch_profile) {
  if (!switch_profile) {
    Reset();
    return;
  }

  // TODO(https://crbug.com/1076880): Switch to the other profile.
  NOTIMPLEMENTED();
  Reset();
}

void DiceWebSigninInterceptor::OnNewSignedInProfileCreated(
    Profile* new_profile) {
  DCHECK(dice_signed_in_profile_creator_);
  dice_signed_in_profile_creator_.reset();

  if (!new_profile) {
    Reset();
    return;
  }

  // Work is done in this profile, the flow continues in the
  // DiceWebSigninInterceptor that is attached to the new profile.
  DiceWebSigninInterceptorFactory::GetForProfile(new_profile)
      ->CreateBrowserAfterSigninInterception(account_id_, web_contents());
  Reset();
}

void DiceWebSigninInterceptor::DeleteSessionStartupHelper() {
  session_startup_helper_.reset();
}
