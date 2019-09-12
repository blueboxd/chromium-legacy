// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/primary_account_policy_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"

PrimaryAccountManager::PrimaryAccountManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    std::unique_ptr<PrimaryAccountPolicyManager> policy_manager)
    : client_(client),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      initialized_(false),
#if !defined(OS_CHROMEOS)
      account_consistency_(account_consistency),
#endif
      policy_manager_(std::move(policy_manager)) {
  DCHECK(client_);
  DCHECK(account_tracker_service_);
}

PrimaryAccountManager::~PrimaryAccountManager() {
  token_service_->RemoveObserver(this);
}

// static
void PrimaryAccountManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesHostedDomain,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastAccountId,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastUsername,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesAccountId, std::string());
  registry->RegisterBooleanPref(prefs::kAutologinEnabled, true);
  registry->RegisterListPref(prefs::kReverseAutologinRejectedEmailList);
  registry->RegisterBooleanPref(prefs::kSigninAllowed, true);
  registry->RegisterBooleanPref(prefs::kSignedInWithCredentialProvider, false);
}

// static
void PrimaryAccountManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                               std::string());
}

void PrimaryAccountManager::Initialize(PrefService* local_state) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  initialized_ = true;

  // If the user is clearing the token service from the command line, then
  // clear their login info also (not valid to be logged in without any
  // tokens).
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kClearTokenService)) {
    client_->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);
  }

  std::string pref_account_id =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesAccountId);

  if (!pref_account_id.empty()) {
    if (account_tracker_service_->GetMigrationState() ==
        AccountTrackerService::MIGRATION_IN_PROGRESS) {
      CoreAccountInfo account_info =
          account_tracker_service_->FindAccountInfoByEmail(pref_account_id);
      // |account_info.gaia| could be empty if |account_id| is already gaia id.
      if (!account_info.gaia.empty()) {
        pref_account_id = account_info.gaia;
        client_->GetPrefs()->SetString(prefs::kGoogleServicesAccountId,
                                       pref_account_id);
      }
    }
    SetAuthenticatedAccountInfo(account_tracker_service_->GetAccountInfo(
        CoreAccountId(pref_account_id)));
  }
  if (policy_manager_) {
    policy_manager_->InitializePolicy(local_state, this);
  }
  // It is important to only load credentials after starting to observe the
  // token service.
  token_service_->AddObserver(this);
  token_service_->LoadCredentials(GetAuthenticatedAccountId());
}

bool PrimaryAccountManager::IsInitialized() const {
  return initialized_;
}

CoreAccountInfo PrimaryAccountManager::GetAuthenticatedAccountInfo() const {
  return authenticated_account_info_.value_or(CoreAccountInfo());
}

CoreAccountId PrimaryAccountManager::GetAuthenticatedAccountId() const {
  return GetAuthenticatedAccountInfo().account_id;
}

void PrimaryAccountManager::SetAuthenticatedAccountInfo(
    const CoreAccountInfo& account_info) {
  DCHECK(!account_info.account_id.empty());
  if (IsAuthenticated()) {
    DCHECK_EQ(account_info.account_id, GetAuthenticatedAccountId())
        << "Changing the authenticated account while authenticated is not "
           "allowed.";
    return;
  }

  std::string pref_account_id =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesAccountId);

  DCHECK(pref_account_id.empty() ||
         pref_account_id == account_info.account_id.id)
      << "account_id=" << account_info.account_id
      << " pref_account_id=" << pref_account_id;
  authenticated_account_info_ = account_info;

  // This preference is set so that code on I/O thread has access to the
  // Gaia id of the signed in user.
  client_->GetPrefs()->SetString(prefs::kGoogleServicesAccountId,
                                 account_info.account_id.id);

  // Go ahead and update the last signed in account info here as well. Once a
  // user is signed in the corresponding preferences should match. Doing it here
  // as opposed to on signin allows us to catch the upgrade scenario.
  client_->GetPrefs()->SetString(prefs::kGoogleServicesLastAccountId,
                                 account_info.account_id.id);
  client_->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                 account_info.email);

  // Commit authenticated account info immediately so that it does not get lost
  // if Chrome crashes before the next commit interval.
  client_->GetPrefs()->CommitPendingWrite();

  if (on_authenticated_account_set_callback_) {
    on_authenticated_account_set_callback_.Run(account_info);
  }
}

void PrimaryAccountManager::ClearAuthenticatedAccountInfo() {
  authenticated_account_info_ = base::nullopt;
  if (on_authenticated_account_cleared_callback_) {
    on_authenticated_account_cleared_callback_.Run();
  }
}

bool PrimaryAccountManager::IsAuthenticated() const {
  return authenticated_account_info_.has_value();
}

void PrimaryAccountManager::SetGoogleSigninSucceededCallback(
    AccountSigninCallback callback) {
  DCHECK(!on_google_signin_succeeded_callback_)
      << "GoogleSigninSucceededCallback shouldn't be set multiple times.";
  on_google_signin_succeeded_callback_ = callback;
}

#if !defined(OS_CHROMEOS)
void PrimaryAccountManager::SetGoogleSignedOutCallback(
    AccountSigninCallback callback) {
  DCHECK(!on_google_signed_out_callback_)
      << "GoogleSignedOutCallback shouldn't be set multiple times.";
  on_google_signed_out_callback_ = callback;
}
#endif  // !defined(OS_CHROMEOS)

void PrimaryAccountManager::SetAuthenticatedAccountSetCallback(
    AccountSigninCallback callback) {
  DCHECK(!on_authenticated_account_set_callback_)
      << "AuthenticatedAccountSetCallback shouldn't be set multiple times.";
  on_authenticated_account_set_callback_ = callback;
}

void PrimaryAccountManager::SetAuthenticatedAccountClearedCallback(
    AccountClearedCallback callback) {
  DCHECK(!on_authenticated_account_cleared_callback_)
      << "AuthenticatedAccountClearedCallback shouldn't be set multiple times.";
  on_authenticated_account_cleared_callback_ = callback;
}

void PrimaryAccountManager::SignIn(const std::string& username) {
  CoreAccountInfo info =
      account_tracker_service_->FindAccountInfoByEmail(username);
  DCHECK(!info.gaia.empty());
  DCHECK(!info.email.empty());

  bool reauth_in_progress = IsAuthenticated();
  SetAuthenticatedAccountInfo(info);

  if (!reauth_in_progress && on_google_signin_succeeded_callback_)
    on_google_signin_succeeded_callback_.Run(GetAuthenticatedAccountInfo());
}

void PrimaryAccountManager::UpdateAuthenticatedAccountInfo() {
  DCHECK(authenticated_account_info_.has_value());
  const CoreAccountInfo info = account_tracker_service_->GetAccountInfo(
      authenticated_account_info_->account_id);
  DCHECK_EQ(info.account_id, authenticated_account_info_->account_id);
  authenticated_account_info_ = info;
}

#if !defined(OS_CHROMEOS)
void PrimaryAccountManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  RemoveAccountsOption remove_option =
      (account_consistency_ == signin::AccountConsistencyMethod::kDice)
          ? RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError
          : RemoveAccountsOption::kRemoveAllAccounts;
  StartSignOut(signout_source_metric, signout_delete_metric, remove_option);
}

void PrimaryAccountManager::SignOutAndRemoveAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kRemoveAllAccounts);
}

void PrimaryAccountManager::SignOutAndKeepAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kKeepAllAccounts);
}

void PrimaryAccountManager::StartSignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option) {
  VLOG(1) << "StartSignOut: " << static_cast<int>(signout_source_metric) << ", "
          << static_cast<int>(signout_delete_metric) << ", "
          << static_cast<int>(remove_option);
  client_->PreSignOut(
      base::BindOnce(&PrimaryAccountManager::OnSignoutDecisionReached,
                     base::Unretained(this), signout_source_metric,
                     signout_delete_metric, remove_option),
      signout_source_metric);
}

void PrimaryAccountManager::OnSignoutDecisionReached(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option,
    SigninClient::SignoutDecision signout_decision) {
  DCHECK(IsInitialized());

  VLOG(1) << "OnSignoutDecisionReached: "
          << (signout_decision == SigninClient::SignoutDecision::ALLOW_SIGNOUT);
  signin_metrics::LogSignout(signout_source_metric, signout_delete_metric);
  if (!IsAuthenticated()) {
    return;
  }

  // TODO(crbug.com/887756): Consider moving this higher up, or document why
  // the above blocks are exempt from the |signout_decision| early return.
  if (signout_decision == SigninClient::SignoutDecision::DISALLOW_SIGNOUT) {
    VLOG(1) << "Ignoring attempt to sign out while signout disallowed";
    return;
  }

  const CoreAccountInfo account_info = GetAuthenticatedAccountInfo();

  ClearAuthenticatedAccountInfo();
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesHostedDomain);
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  switch (remove_option) {
    case RemoveAccountsOption::kRemoveAllAccounts:
      VLOG(0) << "Revoking all refresh tokens on server. Reason: sign out";
      token_service_->RevokeAllCredentials(
          signin_metrics::SourceForRefreshTokenOperation::
              kPrimaryAccountManager_ClearAccount);
      break;
    case RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError:
      if (token_service_->RefreshTokenHasError(account_info.account_id))
        token_service_->RevokeCredentials(
            account_info.account_id,
            signin_metrics::SourceForRefreshTokenOperation::
                kPrimaryAccountManager_ClearAccount);
      break;
    case RemoveAccountsOption::kKeepAllAccounts:
      // Do nothing.
      break;
  }

  FireGoogleSignedOut(account_info);
}

void PrimaryAccountManager::FireGoogleSignedOut(
    const CoreAccountInfo& account_info) {
  if (on_google_signed_out_callback_) {
    on_google_signed_out_callback_.Run(account_info);
  }
}

void PrimaryAccountManager::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);

  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    account_tracker_service_->SetMigrationDone();
  }

  // Remove account information from the account tracker service if needed.
  if (token_service_->HasLoadCredentialsFinishedWithNoErrors()) {
    std::vector<AccountInfo> accounts_in_tracker_service =
        account_tracker_service_->GetAccounts();
    const CoreAccountId authenticated_account_id = GetAuthenticatedAccountId();
    for (const auto& account : accounts_in_tracker_service) {
      if (authenticated_account_id != account.account_id &&
          !token_service_->RefreshTokenIsAvailable(account.account_id)) {
        VLOG(0) << "Removed account from account tracker service: "
                << account.account_id;
        account_tracker_service_->RemoveAccount(account.account_id);
      }
    }
  }
}
#endif  // !defined(OS_CHROMEOS)
