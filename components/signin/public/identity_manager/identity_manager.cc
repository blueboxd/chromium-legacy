// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager.h"

#include <string>

#include "base/bind.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/ubertoken_fetcher_impl.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/diagnostics_provider.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "components/signin/internal/identity_manager/oauth2_token_service_delegate_android.h"
#elif !defined(OS_IOS)
#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"
#endif

namespace signin {

namespace {

// Local copy of the account ID used for supervised users (defined in //chrome
// as supervised_users::kSupervisedUserPseudoEmail). Simply copied to avoid
// plumbing it from //chrome all the way down through the Identity Service just
// to handle the corner cases below.
// TODO(860492): Remove this once supervised user support is removed.
const char kSupervisedUserPseudoEmail[] = "managed_user@localhost";

// A made-up Gaia ID to populate the supervised user's AccountInfo with in order
// to maintain the invariant that the AccountInfos passed out by IdentityManager
// always have an account ID, Gaia ID, and email set.
// TODO(860492): Remove this once supervised user support is removed.
const char kSupervisedUserPseudoGaiaID[] = "managed_user_gaia_id";

}  // namespace

IdentityManager::IdentityManager(
    std::unique_ptr<AccountTrackerService> account_tracker_service,
    std::unique_ptr<ProfileOAuth2TokenService> token_service,
    std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service,
    std::unique_ptr<PrimaryAccountManager> primary_account_manager,
    std::unique_ptr<AccountFetcherService> account_fetcher_service,
    std::unique_ptr<PrimaryAccountMutator> primary_account_mutator,
    std::unique_ptr<AccountsMutator> accounts_mutator,
    std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator,
    std::unique_ptr<DiagnosticsProvider> diagnostics_provider,
    std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer)
    : account_tracker_service_(std::move(account_tracker_service)),
      token_service_(std::move(token_service)),
      gaia_cookie_manager_service_(std::move(gaia_cookie_manager_service)),
      primary_account_manager_(std::move(primary_account_manager)),
      account_fetcher_service_(std::move(account_fetcher_service)),
      primary_account_mutator_(std::move(primary_account_mutator)),
      accounts_mutator_(std::move(accounts_mutator)),
      accounts_cookie_mutator_(std::move(accounts_cookie_mutator)),
      diagnostics_provider_(std::move(diagnostics_provider)),
      device_accounts_synchronizer_(std::move(device_accounts_synchronizer)) {
  DCHECK(account_fetcher_service_);
  DCHECK(accounts_cookie_mutator_);
  DCHECK(diagnostics_provider_);

  DCHECK(!accounts_mutator_ || !device_accounts_synchronizer_)
      << "Cannot have both an AccountsMutator and a DeviceAccountsSynchronizer";

  // IdentityManager will outlive the PrimaryAccountManager, so base::Unretained
  // is safe.
  primary_account_manager_->SetGoogleSigninSucceededCallback(
      base::BindRepeating(&IdentityManager::GoogleSigninSucceeded,
                          base::Unretained(this)));
  primary_account_manager_->SetAuthenticatedAccountSetCallback(
      base::BindRepeating(&IdentityManager::AuthenticatedAccountSet,
                          base::Unretained(this)));
  primary_account_manager_->SetAuthenticatedAccountClearedCallback(
      base::BindRepeating(&IdentityManager::AuthenticatedAccountCleared,
                          base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  primary_account_manager_->SetGoogleSignedOutCallback(base::BindRepeating(
      &IdentityManager::GoogleSignedOut, base::Unretained(this)));
#endif

  token_service_->AddObserver(this);
  token_service_->AddAccessTokenDiagnosticsObserver(this);

  // IdentityManager owns the ATS, GCMS and PO2TS instances and will outlive
  // them, so base::Unretained is safe.
  account_tracker_service_->SetOnAccountUpdatedCallback(base::BindRepeating(
      &IdentityManager::OnAccountUpdated, base::Unretained(this)));
  account_tracker_service_->SetOnAccountRemovedCallback(base::BindRepeating(
      &IdentityManager::OnAccountRemoved, base::Unretained(this)));
  gaia_cookie_manager_service_->SetGaiaAccountsInCookieUpdatedCallback(
      base::BindRepeating(&IdentityManager::OnGaiaAccountsInCookieUpdated,
                          base::Unretained(this)));
  gaia_cookie_manager_service_->SetGaiaCookieDeletedByUserActionCallback(
      base::BindRepeating(&IdentityManager::OnGaiaCookieDeletedByUserAction,
                          base::Unretained(this)));
  token_service_->SetRefreshTokenAvailableFromSourceCallback(
      base::BindRepeating(&IdentityManager::OnRefreshTokenAvailableFromSource,
                          base::Unretained(this)));
  token_service_->SetRefreshTokenRevokedFromSourceCallback(
      base::BindRepeating(&IdentityManager::OnRefreshTokenRevokedFromSource,
                          base::Unretained(this)));

  // Seed the primary account with any state that |primary_account_manager_|
  // loaded from prefs.
  if (primary_account_manager_->IsAuthenticated()) {
    CoreAccountInfo account =
        primary_account_manager_->GetAuthenticatedAccountInfo();
    DCHECK(!account.account_id.empty());
    SetPrimaryAccountInternal(std::move(account));
  }
}

IdentityManager::~IdentityManager() {
  account_fetcher_service_->Shutdown();
  gaia_cookie_manager_service_->Shutdown();
  token_service_->Shutdown();
  account_tracker_service_->Shutdown();

  token_service_->RemoveObserver(this);
  token_service_->RemoveAccessTokenDiagnosticsObserver(this);
}

void IdentityManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// TODO(862619) change return type to base::Optional<CoreAccountInfo>
CoreAccountInfo IdentityManager::GetPrimaryAccountInfo() const {
  DCHECK_EQ(primary_account_.has_value(),
            primary_account_manager_->IsAuthenticated());
  auto result = primary_account_.value_or(CoreAccountInfo());
  DCHECK_EQ(result.account_id,
            primary_account_manager_->GetAuthenticatedAccountId());
#if DCHECK_IS_ON()
  CoreAccountInfo primary_account_manager_account =
      primary_account_manager_->GetAuthenticatedAccountInfo();
  if (!primary_account_manager_account.account_id.empty()) {
    DCHECK_EQ(primary_account_manager_account, result)
        << "If primary_account_manager_'s account is set (account has a "
           "refresh token), primary_account_ must have the same value.";
  }
#endif
  return result;
}

CoreAccountId IdentityManager::GetPrimaryAccountId() const {
  return GetPrimaryAccountInfo().account_id;
}

bool IdentityManager::HasPrimaryAccount() const {
  DCHECK_EQ(primary_account_.has_value(),
            primary_account_manager_->IsAuthenticated());
  return primary_account_.has_value();
}

CoreAccountId IdentityManager::GetUnconsentedPrimaryAccountId() const {
  return GetUnconsentedPrimaryAccountInfo().account_id;
}

CoreAccountInfo IdentityManager::GetUnconsentedPrimaryAccountInfo() const {
  return unconsented_primary_account_.value_or(CoreAccountInfo());
}

bool IdentityManager::HasUnconsentedPrimaryAccount() const {
  return unconsented_primary_account_.has_value();
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    const identity::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(account_id, oauth_consumer_name,
                                              token_service_.get(), scopes,
                                              std::move(callback), mode);
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const identity::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(
      account_id, oauth_consumer_name, token_service_.get(), url_loader_factory,
      scopes, std::move(callback), mode);
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForClient(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& oauth_consumer_name,
    const identity::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(
      account_id, client_id, client_secret, oauth_consumer_name,
      token_service_.get(), scopes, std::move(callback), mode);
}

void IdentityManager::RemoveAccessTokenFromCache(
    const CoreAccountId& account_id,
    const identity::ScopeSet& scopes,
    const std::string& access_token) {
  token_service_->InvalidateAccessToken(account_id, scopes, access_token);
}

std::vector<CoreAccountInfo> IdentityManager::GetAccountsWithRefreshTokens()
    const {
  std::vector<CoreAccountId> account_ids_with_tokens =
      token_service_->GetAccounts();

  std::vector<CoreAccountInfo> accounts;
  accounts.reserve(account_ids_with_tokens.size());

  for (const CoreAccountId& account_id : account_ids_with_tokens) {
    accounts.push_back(GetAccountInfoForAccountWithRefreshToken(account_id));
  }

  return accounts;
}

std::vector<AccountInfo>
IdentityManager::GetExtendedAccountInfoForAccountsWithRefreshToken() const {
  std::vector<CoreAccountId> account_ids_with_tokens =
      token_service_->GetAccounts();

  std::vector<AccountInfo> accounts;
  accounts.reserve(account_ids_with_tokens.size());

  for (const CoreAccountId& account_id : account_ids_with_tokens) {
    accounts.push_back(GetAccountInfoForAccountWithRefreshToken(account_id));
  }

  return accounts;
}

bool IdentityManager::HasPrimaryAccountWithRefreshToken() const {
  return HasAccountWithRefreshToken(GetPrimaryAccountId());
}

bool IdentityManager::HasAccountWithRefreshToken(
    const CoreAccountId& account_id) const {
  return token_service_->RefreshTokenIsAvailable(account_id);
}

bool IdentityManager::AreRefreshTokensLoaded() const {
  return token_service_->AreAllCredentialsLoaded();
}

bool IdentityManager::HasAccountWithRefreshTokenInPersistentErrorState(
    const CoreAccountId& account_id) const {
  return GetErrorStateOfRefreshTokenForAccount(account_id).IsPersistentError();
}

GoogleServiceAuthError IdentityManager::GetErrorStateOfRefreshTokenForAccount(
    const CoreAccountId& account_id) const {
  return token_service_->GetAuthError(account_id);
}

base::Optional<AccountInfo> IdentityManager::FindExtendedAccountInfoForAccount(
    const CoreAccountInfo& account_info) const {
  AccountInfo extended_account_info =
      account_tracker_service_->GetAccountInfo(account_info.account_id);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(extended_account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

base::Optional<AccountInfo>
IdentityManager::FindAccountInfoForAccountWithRefreshTokenByAccountId(
    const CoreAccountId& account_id) const {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

base::Optional<AccountInfo>
IdentityManager::FindAccountInfoForAccountWithRefreshTokenByEmailAddress(
    const std::string& email_address) const {
  AccountInfo account_info =
      account_tracker_service_->FindAccountInfoByEmail(email_address);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

base::Optional<AccountInfo>
IdentityManager::FindAccountInfoForAccountWithRefreshTokenByGaiaId(
    const std::string& gaia_id) const {
  AccountInfo account_info =
      account_tracker_service_->FindAccountInfoByGaiaId(gaia_id);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

std::unique_ptr<UbertokenFetcher>
IdentityManager::CreateUbertokenFetcherForAccount(
    const CoreAccountId& account_id,
    UbertokenFetcher::CompletionCallback callback,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool bount_to_channel_id) {
  return std::make_unique<UbertokenFetcherImpl>(
      account_id, token_service_.get(), std::move(callback), source,
      url_loader_factory, bount_to_channel_id);
}

AccountsInCookieJarInfo IdentityManager::GetAccountsInCookieJar() const {
  std::vector<gaia::ListedAccount> signed_in_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  bool accounts_are_fresh = gaia_cookie_manager_service_->ListAccounts(
      &signed_in_accounts, &signed_out_accounts);

  return AccountsInCookieJarInfo(accounts_are_fresh, signed_in_accounts,
                                 signed_out_accounts);
}

PrimaryAccountMutator* IdentityManager::GetPrimaryAccountMutator() {
  return primary_account_mutator_.get();
}

AccountsMutator* IdentityManager::GetAccountsMutator() {
  return accounts_mutator_.get();
}

AccountsCookieMutator* IdentityManager::GetAccountsCookieMutator() {
  return accounts_cookie_mutator_.get();
}

DeviceAccountsSynchronizer* IdentityManager::GetDeviceAccountsSynchronizer() {
  return device_accounts_synchronizer_.get();
}

void IdentityManager::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

void IdentityManager::OnNetworkInitialized() {
  gaia_cookie_manager_service_->InitCookieListener();
  account_fetcher_service_->OnNetworkInitialized();
}

// static
bool IdentityManager::IsAccountIdMigrationSupported() {
  return AccountTrackerService::IsMigrationSupported();
}

// static
void IdentityManager::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  PrimaryAccountManager::RegisterPrefs(registry);
}

// static
void IdentityManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  ProfileOAuth2TokenService::RegisterProfilePrefs(registry);
  PrimaryAccountManager::RegisterProfilePrefs(registry);
  AccountFetcherService::RegisterPrefs(registry);
  AccountTrackerService::RegisterPrefs(registry);
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(registry);
#endif
}

CoreAccountId IdentityManager::PickAccountIdForAccount(
    const std::string& gaia,
    const std::string& email) const {
  // TODO(triploblastic@): Remove explicit conversion once
  // primary_account_manager has been fixed to use CoreAccountId.
  return CoreAccountId(
      account_tracker_service_->PickAccountIdForAccount(gaia, email));
}

IdentityManager::AccountIdMigrationState
IdentityManager::GetAccountIdMigrationState() const {
  return static_cast<IdentityManager::AccountIdMigrationState>(
      account_tracker_service_->GetMigrationState());
}

#if !defined(OS_IOS) && !defined(OS_ANDROID)
void IdentityManager::DeprecatedLoadCredentialsForSupervisedUser(
    const CoreAccountId& primary_account_id) {
  token_service_->LoadCredentials(primary_account_id);
}
#endif

DiagnosticsProvider* IdentityManager::GetDiagnosticsProvider() {
  return diagnostics_provider_.get();
}

#if defined(OS_IOS)
void IdentityManager::ForceTriggerOnCookieChange() {
  gaia_cookie_manager_service_->ForceOnCookieChangeProcessing();
}
#endif

#if defined(OS_ANDROID)
void IdentityManager::LegacyReloadAccountsFromSystem() {
  token_service_->GetDelegate()->ReloadAccountsFromSystem(
      GetPrimaryAccountId());
}

base::android::ScopedJavaLocalRef<jobject>
IdentityManager::LegacyGetAccountTrackerServiceJavaObject() {
  return account_tracker_service_->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject>
IdentityManager::LegacyGetOAuth2TokenServiceJavaObject() {
  OAuth2TokenServiceDelegateAndroid* delegate =
      static_cast<OAuth2TokenServiceDelegateAndroid*>(
          token_service_->GetDelegate());
  return delegate->GetJavaObject();
}

void IdentityManager::ForceRefreshOfExtendedAccountInfo(
    const CoreAccountId& account_id) {
  DCHECK(HasAccountWithRefreshToken(account_id));
  account_fetcher_service_->ForceRefreshOfAccountInfo(account_id);
}
#endif

PrimaryAccountManager* IdentityManager::GetPrimaryAccountManager() {
  return primary_account_manager_.get();
}

ProfileOAuth2TokenService* IdentityManager::GetTokenService() {
  return token_service_.get();
}

AccountTrackerService* IdentityManager::GetAccountTrackerService() {
  return account_tracker_service_.get();
}

AccountFetcherService* IdentityManager::GetAccountFetcherService() {
  return account_fetcher_service_.get();
}

GaiaCookieManagerService* IdentityManager::GetGaiaCookieManagerService() {
  return gaia_cookie_manager_service_.get();
}

AccountInfo IdentityManager::GetAccountInfoForAccountWithRefreshToken(
    const CoreAccountId& account_id) const {
  // TODO(https://crbug.com/919793): This invariant is not currently possible to
  // enforce on Android due to the underlying relationship between
  // O2TS::GetAccounts(), O2TS::RefreshTokenIsAvailable(), and
  // O2TS::Observer::OnRefreshTokenAvailable().
#if !defined(OS_ANDROID)
  DCHECK(HasAccountWithRefreshToken(account_id));
#endif

  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  // In the context of supervised users, the ProfileOAuth2TokenService is used
  // without the AccountTrackerService being used. This is the only case in
  // which the AccountTrackerService will potentially not know about the
  // account. In this context, |account_id| is always set to
  // kSupervisedUserPseudoEmail. Populate the information manually in this case
  // to maintain the invariant that the account ID, gaia ID, and email are
  // always set.
  // TODO(860492): Remove this special case once supervised user support is
  // removed.
  DCHECK(!account_info.IsEmpty() ||
         account_id.id == kSupervisedUserPseudoEmail);
  if (account_id.id == kSupervisedUserPseudoEmail && account_info.IsEmpty()) {
    account_info.account_id = account_id;
    account_info.email = kSupervisedUserPseudoEmail;
    account_info.gaia = kSupervisedUserPseudoGaiaID;
  }

  return account_info;
}

void IdentityManager::SetPrimaryAccountInternal(
    base::Optional<CoreAccountInfo> account_info) {
  primary_account_ = std::move(account_info);
  UpdateUnconsentedPrimaryAccount();
}

void IdentityManager::UpdateUnconsentedPrimaryAccount() {
  base::Optional<CoreAccountInfo> new_unconsented_primary_account =
      ComputeUnconsentedPrimaryAccountInfo();
  if (unconsented_primary_account_ != new_unconsented_primary_account) {
    unconsented_primary_account_ = std::move(new_unconsented_primary_account);
    for (auto& observer : observer_list_) {
      observer.OnUnconsentedPrimaryAccountChanged(
          unconsented_primary_account_.value_or(CoreAccountInfo()));
    }
  }
}

base::Optional<CoreAccountInfo>
IdentityManager::ComputeUnconsentedPrimaryAccountInfo() const {
  if (HasPrimaryAccount())
    return GetPrimaryAccountInfo();

#if defined(OS_CHROMEOS) || defined(OS_IOS) || defined(OS_ANDROID)
  // On ChromeOS and on mobile platforms, we support only the primary account as
  // the unconsented primary account. By this early return, we avoid an extra
  // request to GAIA that lists cookie accounts.
  return base::nullopt;
#else
  std::vector<gaia::ListedAccount> cookie_accounts =
      GetAccountsInCookieJar().signed_in_accounts;
  if (cookie_accounts.empty())
    return base::nullopt;

  const CoreAccountId first_account_id = cookie_accounts[0].id;
  if (!HasAccountWithRefreshToken(first_account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(first_account_id);
#endif
}

void IdentityManager::GoogleSigninSucceeded(const AccountInfo& account_info) {
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountSet(account_info);
  }
}

void IdentityManager::GoogleSignedOut(const AccountInfo& account_info) {
  DCHECK(!HasPrimaryAccount());
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountCleared(account_info);
  }
}
void IdentityManager::AuthenticatedAccountSet(const AccountInfo& account_info) {
  DCHECK(primary_account_manager_->IsAuthenticated());
  SetPrimaryAccountInternal(account_info);
}
void IdentityManager::AuthenticatedAccountCleared() {
  DCHECK(!primary_account_manager_->IsAuthenticated());
  SetPrimaryAccountInternal(base::nullopt);
}

void IdentityManager::OnRefreshTokenAvailable(const CoreAccountId& account_id) {
  UpdateUnconsentedPrimaryAccount();
  CoreAccountInfo account_info =
      GetAccountInfoForAccountWithRefreshToken(account_id);

  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenUpdatedForAccount(account_info);
  }
}

void IdentityManager::OnRefreshTokenRevoked(const CoreAccountId& account_id) {
  UpdateUnconsentedPrimaryAccount();
  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenRemovedForAccount(account_id);
  }
}

void IdentityManager::OnRefreshTokensLoaded() {
  UpdateUnconsentedPrimaryAccount();
  for (auto& observer : observer_list_)
    observer.OnRefreshTokensLoaded();
}

void IdentityManager::OnEndBatchChanges() {
  for (auto& observer : observer_list_)
    observer.OnEndBatchOfRefreshTokenStateChanges();
}

void IdentityManager::OnAuthErrorChanged(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  CoreAccountInfo account_info =
      GetAccountInfoForAccountWithRefreshToken(account_id);

  for (auto& observer : observer_list_)
    observer.OnErrorStateOfRefreshTokenUpdatedForAccount(account_info,
                                                         auth_error);
}

void IdentityManager::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& signed_in_accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  UpdateUnconsentedPrimaryAccount();
  AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      error == GoogleServiceAuthError::AuthErrorNone(), signed_in_accounts,
      signed_out_accounts);

  for (auto& observer : observer_list_) {
    observer.OnAccountsInCookieUpdated(accounts_in_cookie_jar_info, error);
  }
}

void IdentityManager::OnGaiaCookieDeletedByUserAction() {
  UpdateUnconsentedPrimaryAccount();
  for (auto& observer : observer_list_) {
    observer.OnAccountsCookieDeletedByUserAction();
  }
}

void IdentityManager::OnAccessTokenRequested(const CoreAccountId& account_id,
                                             const std::string& consumer_id,
                                             const identity::ScopeSet& scopes) {
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnAccessTokenRequested(account_id, consumer_id, scopes);
  }
}

void IdentityManager::OnFetchAccessTokenComplete(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const identity::ScopeSet& scopes,
    GoogleServiceAuthError error,
    base::Time expiration_time) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnAccessTokenRequestCompleted(account_id, consumer_id, scopes,
                                           error, expiration_time);
}

void IdentityManager::OnAccessTokenRemoved(const CoreAccountId& account_id,
                                           const identity::ScopeSet& scopes) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnAccessTokenRemovedFromCache(account_id, scopes);
}

void IdentityManager::OnRefreshTokenAvailableFromSource(
    const CoreAccountId& account_id,
    bool is_refresh_token_valid,
    const std::string& source) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnRefreshTokenUpdatedForAccountFromSource(
        account_id, is_refresh_token_valid, source);
}

void IdentityManager::OnRefreshTokenRevokedFromSource(
    const CoreAccountId& account_id,
    const std::string& source) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnRefreshTokenRemovedForAccountFromSource(account_id, source);
}

void IdentityManager::OnAccountUpdated(const AccountInfo& info) {
  if (primary_account_ && primary_account_->account_id == info.account_id) {
    SetPrimaryAccountInternal(info);
  }
  for (auto& observer : observer_list_) {
    observer.OnExtendedAccountInfoUpdated(info);
  }
}

void IdentityManager::OnAccountRemoved(const AccountInfo& info) {
  for (auto& observer : observer_list_)
    observer.OnExtendedAccountInfoRemoved(info);
  UpdateUnconsentedPrimaryAccount();
}

}  // namespace signin
