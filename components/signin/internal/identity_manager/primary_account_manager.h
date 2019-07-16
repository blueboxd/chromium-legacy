// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The PrimaryAccountManager encapsulates some functionality tracking
// which account is the primary one.
//
// **NOTE** on semantics of PrimaryAccountManager:
//
// Once a signin is successful, the username becomes "established" and will not
// be cleared until a SignOut operation is performed (persists across
// restarts). Until that happens, the primary account manager can still be used
// to refresh credentials, but changing the username is not permitted.
//
// On ChromeOS signout is not possible, so that functionality is if-def'd out on
// that platform.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MANAGER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/oauth2_token_service_observer.h"

struct AccountInfo;
class AccountTrackerService;
class PrefRegistrySimple;
class PrefService;
class PrimaryAccountPolicyManager;
class ProfileOAuth2TokenService;

namespace signin_metrics {
enum ProfileSignout : int;
enum class SignoutDelete;
}  // namespace signin_metrics

class PrimaryAccountManager : public OAuth2TokenServiceObserver {
 public:
  typedef base::RepeatingCallback<void(const AccountInfo&)>
      AccountSigninCallback;
  typedef base::RepeatingCallback<void()> AccountClearedCallback;

#if !defined(OS_CHROMEOS)
  // Used to remove accounts from the token service and the account tracker.
  enum class RemoveAccountsOption {
    // Do not remove accounts.
    kKeepAllAccounts = 0,
    // Remove all the accounts.
    kRemoveAllAccounts,
    // Removes the authenticated account if it is in authentication error.
    kRemoveAuthenticatedAccountIfInError
  };
#endif

  PrimaryAccountManager(
      SigninClient* client,
      ProfileOAuth2TokenService* token_service,
      AccountTrackerService* account_tracker_service,
      signin::AccountConsistencyMethod account_consistency,
      std::unique_ptr<PrimaryAccountPolicyManager> policy_manager);
  ~PrimaryAccountManager() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers per-install prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // If user was signed in, load tokens from DB if available.
  void Initialize(PrefService* local_state);
  bool IsInitialized() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the know information of the account. Otherwise, it returns an empty struct.
  AccountInfo GetAuthenticatedAccountInfo() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the account id. Otherwise, it returns an empty string.  This id is the
  // G+/Focus obfuscated gaia id of the user. It can be used to uniquely
  // identify an account, so for example as a key to map accounts to data. For
  // code that needs a unique id to represent the connected account, call this
  // method. Example: the AccountStatusMap type in
  // MutableProfileOAuth2TokenService. For code that needs to know the
  // normalized email address of the connected account, use
  // GetAuthenticatedAccountInfo().email.  Example: to show the string "Signed
  // in as XXX" in the hotdog menu.
  const CoreAccountId& GetAuthenticatedAccountId() const;

  // Sets the authenticated user's Gaia ID and display email.  Internally,
  // this will seed the account information in AccountTrackerService and pick
  // the right account_id for this account.
  void SetAuthenticatedAccountInfo(const std::string& gaia_id,
                                   const std::string& email);

  // Returns true if there is an authenticated user.
  bool IsAuthenticated() const;

  // If set, this callback will be invoked whenever a user signs into Google
  // services such as sync. This callback is not called during a reauth.
  void SetGoogleSigninSucceededCallback(AccountSigninCallback callback);

#if !defined(OS_CHROMEOS)
  // If set, this callback will be invoked whenever the currently signed-in user
  // for a user has been signed out.
  void SetGoogleSignedOutCallback(AccountSigninCallback callback);
#endif

  // If set, this callback will be invoked during the signin as soon as
  // PrimaryAccountManager::authenticated_account_id_ is set.
  void SetAuthenticatedAccountSetCallback(AccountSigninCallback callback);

  // If set, this callback will be invoked during the signout as soon as
  // PrimaryAccountManager::authenticated_account_id_ is cleared.
  void SetAuthenticatedAccountClearedCallback(AccountClearedCallback callback);

  // Signs a user in. PrimaryAccountManager assumes that |username| can be used
  // to look up the corresponding account_id and gaia_id for this email.
  void SignIn(const std::string& username);

  // Signout API surfaces (not supported on ChromeOS, where signout is not
  // permitted).
#if !defined(OS_CHROMEOS)
  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // On mobile and on desktop pre-DICE, this also removes all accounts from
  // Chrome by revoking all refresh tokens.
  // On desktop with DICE enabled, this will remove the authenticated account
  // from Chrome only if it is in authentication error. No other accounts are
  // removed.
  void SignOut(signin_metrics::ProfileSignout signout_source_metric,
               signin_metrics::SignoutDelete signout_delete_metric);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // It removes all accounts from Chrome by revoking all refresh tokens.
  void SignOutAndRemoveAllAccounts(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // Does not remove the accounts from the token service.
  void SignOutAndKeepAllAccounts(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric);
#endif

 private:
  // Sets the authenticated user's account id.
  // If the user is already authenticated with the same account id, then this
  // method is a no-op.
  // It is forbidden to call this method if the user is already authenticated
  // with a different account (this method will DCHECK in that case).
  // |account_id| must not be empty. To log the user out, use
  // ClearAuthenticatedAccountId() instead.
  void SetAuthenticatedAccountId(const CoreAccountId& account_id);

  // Clears the authenticated user's account id.
  // This method is not public because PrimaryAccountManager does not allow
  // signing out by default. Subclasses implementing a sign-out functionality
  // need to call this.
  void ClearAuthenticatedAccountId();

#if !defined(OS_CHROMEOS)
  // Starts the sign out process.
  void StartSignOut(signin_metrics::ProfileSignout signout_source_metric,
                    signin_metrics::SignoutDelete signout_delete_metric,
                    RemoveAccountsOption remove_option);

  // The sign out process which is started by SigninClient::PreSignOut()
  void OnSignoutDecisionReached(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric,
      RemoveAccountsOption remove_option,
      SigninClient::SignoutDecision signout_decision);

  // Send all observers |GoogleSignedOut| notifications.
  void FireGoogleSignedOut(const AccountInfo& account_info);

  // OAuth2TokenServiceObserver:
  void OnRefreshTokensLoaded() override;
#endif

  SigninClient* client_;

  // The ProfileOAuth2TokenService instance associated with this object. Must
  // outlive this object.
  ProfileOAuth2TokenService* token_service_;

  AccountTrackerService* account_tracker_service_;

  bool initialized_;

  // Account id after successful authentication.
  CoreAccountId authenticated_account_id_;

  // Callbacks which will be invoked, if set, for signin-related events.
  AccountSigninCallback on_google_signin_succeeded_callback_;
#if !defined(OS_CHROMEOS)
  AccountSigninCallback on_google_signed_out_callback_;
#endif
  AccountSigninCallback on_authenticated_account_set_callback_;
  AccountClearedCallback on_authenticated_account_cleared_callback_;

  // The list of callbacks notified on shutdown.
  base::CallbackList<void()> on_shutdown_callback_list_;

#if !defined(OS_CHROMEOS)
  signin::AccountConsistencyMethod account_consistency_;
#endif

  std::unique_ptr<PrimaryAccountPolicyManager> policy_manager_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryAccountManager);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MANAGER_H_
