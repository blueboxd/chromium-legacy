// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_oauth2_token_service.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/signin_pref_names.h"

using signin_metrics::SourceForRefreshTokenOperation;

namespace {
std::string SourceToString(SourceForRefreshTokenOperation source) {
  switch (source) {
    case SourceForRefreshTokenOperation::kUnknown:
      return "Unknown";
    case SourceForRefreshTokenOperation::kTokenService_LoadCredentials:
      return "TokenService::LoadCredentials";
    case SourceForRefreshTokenOperation::kSupervisedUser_InitSync:
      return "SupervisedUser::InitSync";
    case SourceForRefreshTokenOperation::kInlineLoginHandler_Signin:
      return "InlineLoginHandler::Signin";
    case SourceForRefreshTokenOperation::kPrimaryAccountManager_ClearAccount:
      return "PrimaryAccountManager::ClearAccount";
    case SourceForRefreshTokenOperation::
        kPrimaryAccountManager_LegacyPreDiceSigninFlow:
      return "PrimaryAccountManager::LegacyPreDiceSigninFlow";
    case SourceForRefreshTokenOperation::kUserMenu_RemoveAccount:
      return "UserMenu::RemoveAccount";
    case SourceForRefreshTokenOperation::kUserMenu_SignOutAllAccounts:
      return "UserMenu::SignOutAllAccounts";
    case SourceForRefreshTokenOperation::kSettings_Signout:
      return "Settings::Signout";
    case SourceForRefreshTokenOperation::kSettings_PauseSync:
      return "Settings::PauseSync";
    case SourceForRefreshTokenOperation::
        kAccountReconcilor_GaiaCookiesDeletedByUser:
      return "AccountReconcilor::GaiaCookiesDeletedByUser";
    case SourceForRefreshTokenOperation::kAccountReconcilor_GaiaCookiesUpdated:
      return "AccountReconcilor::GaiaCookiesUpdated";
    case SourceForRefreshTokenOperation::kAccountReconcilor_Reconcile:
      return "AccountReconcilor::Reconcile";
    case SourceForRefreshTokenOperation::kDiceResponseHandler_Signin:
      return "DiceResponseHandler::Signin";
    case SourceForRefreshTokenOperation::kDiceResponseHandler_Signout:
      return "DiceResponseHandler::Signout";
    case SourceForRefreshTokenOperation::kDiceTurnOnSyncHelper_Abort:
      return "DiceTurnOnSyncHelper::Abort";
    case SourceForRefreshTokenOperation::kMachineLogon_CredentialProvider:
      return "MachineLogon::CredentialProvider";
    case SourceForRefreshTokenOperation::kTokenService_ExtractCredentials:
      return "TokenService::ExtractCredentials";
  }
}
}  // namespace

ProfileOAuth2TokenService::ProfileOAuth2TokenService(
    PrefService* user_prefs,
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : OAuth2TokenService(std::move(delegate)), user_prefs_(user_prefs) {
  DCHECK(user_prefs_);
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
}

// static
void ProfileOAuth2TokenService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
#if defined(OS_IOS)
  registry->RegisterBooleanPref(prefs::kTokenServiceExcludeAllSecondaryAccounts,
                                false);
  registry->RegisterListPref(prefs::kTokenServiceExcludedSecondaryAccounts);
#endif
  registry->RegisterStringPref(prefs::kGoogleServicesSigninScopedDeviceId,
                               std::string());
}

void ProfileOAuth2TokenService::Shutdown() {
  CancelAllRequests();
  GetDelegate()->Shutdown();
}

void ProfileOAuth2TokenService::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  DCHECK_EQ(SourceForRefreshTokenOperation::kUnknown,
            update_refresh_token_source_);
  update_refresh_token_source_ =
      SourceForRefreshTokenOperation::kTokenService_LoadCredentials;
  GetDelegate()->LoadCredentials(primary_account_id);
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token,
    SourceForRefreshTokenOperation source) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_, source);
  GetDelegate()->UpdateCredentials(account_id, refresh_token);
}

void ProfileOAuth2TokenService::RevokeCredentials(
    const CoreAccountId& account_id,
    SourceForRefreshTokenOperation source) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_, source);
  GetDelegate()->RevokeCredentials(account_id);
}

void ProfileOAuth2TokenService::RevokeAllCredentials(
    SourceForRefreshTokenOperation source) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_, source);
  CancelAllRequests();
  ClearCache();
  GetDelegate()->RevokeAllCredentials();
}

const net::BackoffEntry* ProfileOAuth2TokenService::GetDelegateBackoffEntry() {
  return GetDelegate()->BackoffEntry();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfileOAuth2TokenService::ExtractCredentials(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_,
      SourceForRefreshTokenOperation::kTokenService_ExtractCredentials);
  GetDelegate()->ExtractCredentials(to_service, account_id);
}
#endif

void ProfileOAuth2TokenService::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  OAuth2TokenService::OnRefreshTokenAvailable(account_id);

  // Check if the newly-updated token is valid (invalid tokens are inserted when
  // the user signs out on the web with DICE enabled).
  bool is_valid = true;
  GoogleServiceAuthError token_error = GetAuthError(account_id);
  if (token_error == GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_REJECTED_BY_CLIENT)) {
    is_valid = false;
  }

  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);

  signin_metrics::RecordRefreshTokenUpdatedFromSource(
      is_valid, update_refresh_token_source_);

  std::string source_string = SourceToString(update_refresh_token_source_);
  for (auto& diagnostic_observer : GetDiagnicsObservers()) {
    diagnostic_observer.OnRefreshTokenAvailableFromSource(account_id, is_valid,
                                                          source_string);
  }
}

void ProfileOAuth2TokenService::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  OAuth2TokenService::OnRefreshTokenRevoked(account_id);

  // If this was the last token, recreate the device ID.
  RecreateDeviceIdIfNeeded();

  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);

  signin_metrics::RecordRefreshTokenRevokedFromSource(
      update_refresh_token_source_);
  std::string source_string = SourceToString(update_refresh_token_source_);
  for (auto& diagnostic_observer : GetDiagnicsObservers()) {
    diagnostic_observer.OnRefreshTokenRevokedFromSource(account_id,
                                                        source_string);
  }
}

void ProfileOAuth2TokenService::OnRefreshTokensLoaded() {
  OAuth2TokenService::OnRefreshTokensLoaded();

  DCHECK_NE(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_NOT_STARTED,
            GetDelegate()->load_credentials_state());
  DCHECK_NE(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS,
            GetDelegate()->load_credentials_state());

  // Reset the state for update refresh token operations to Unknown as this
  // was the original state before LoadCredentials was called.
  update_refresh_token_source_ = SourceForRefreshTokenOperation::kUnknown;

  // Ensure the device ID is not empty, and recreate it if all tokens were
  // cleared during the loading process.
  RecreateDeviceIdIfNeeded();
}

bool ProfileOAuth2TokenService::HasLoadCredentialsFinishedWithNoErrors() {
  switch (GetDelegate()->load_credentials_state()) {
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_NOT_STARTED:
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS:
      // LoadCredentials has not finished.
      return false;
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS:
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS:
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS:
      // LoadCredentials finished, but with errors
      return false;
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS:
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT:
      // Load credentials finished with success.
      return true;
  }
}

void ProfileOAuth2TokenService::RecreateDeviceIdIfNeeded() {
// On ChromeOS the device ID is not managed by the token service.
#if !defined(OS_CHROMEOS)
  if (AreAllCredentialsLoaded() && HasLoadCredentialsFinishedWithNoErrors() &&
      GetAccounts().empty()) {
    signin::RecreateSigninScopedDeviceId(user_prefs_);
  }
#endif
}
