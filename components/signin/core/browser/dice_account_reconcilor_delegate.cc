// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace signin {

DiceAccountReconcilorDelegate::DiceAccountReconcilorDelegate(
    SigninClient* signin_client,
    AccountConsistencyMethod account_consistency)
    : signin_client_(signin_client), account_consistency_(account_consistency) {
  DCHECK(signin_client_);
  DCHECK(DiceMethodGreaterOrEqual(account_consistency_,
                                  AccountConsistencyMethod::kDiceMigration));
}

bool DiceAccountReconcilorDelegate::IsReconcileEnabled() const {
  return true;
}

bool DiceAccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return account_consistency_ == AccountConsistencyMethod::kDice;
}

gaia::GaiaSource DiceAccountReconcilorDelegate::GetGaiaApiSource() const {
  return gaia::GaiaSource::kAccountReconcilorDice;
}

// - On first execution, the candidates are examined in this order:
//   1. The primary account
//   2. The current first Gaia account
//   3. The last known first Gaia account
//   4. The first account in the token service
// - On subsequent executions, the order is:
//   1. The current first Gaia account
//   2. The primary account
//   3. The last known first Gaia account
//   4. The first account in the token service
CoreAccountId DiceAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const CoreAccountId& primary_account,
    bool first_execution,
    bool will_logout) const {
  bool primary_account_has_token =
      !primary_account.empty() &&
      base::Contains(chrome_accounts, primary_account);

  if (gaia_accounts.empty()) {
    if (primary_account_has_token)
      return primary_account;

    // Try the last known account. This happens when the cookies are cleared
    // while Sync is disabled.
    if (base::Contains(chrome_accounts, last_known_first_account_))
      return last_known_first_account_;

    // As a last resort, use the first Chrome account.
    return chrome_accounts.empty() ? CoreAccountId() : chrome_accounts[0];
  }

  const CoreAccountId& first_gaia_account = gaia_accounts[0].id;
  bool first_gaia_account_has_token =
      base::Contains(chrome_accounts, first_gaia_account);

  if (!first_gaia_account_has_token &&
      (primary_account == first_gaia_account) && gaia_accounts[0].valid) {
    // The primary account is also the first Gaia account, and has no token.
    // Logout everything.
    return CoreAccountId();
  }

  // If the primary Chrome account and the default Gaia account are both in
  // error, then the first gaia account can be kept, to avoid logging the user
  // out of their other accounts.
  // It's only possible when the reconcilor will not perform a logout, because
  // that account cannot be rebuilt.
  if (!first_gaia_account_has_token && !gaia_accounts[0].valid && !will_logout)
    return first_gaia_account;

  if (first_execution) {
    // On first execution, try the primary account, and then the first Gaia
    // account.
    if (primary_account_has_token)
      return primary_account;
    if (first_gaia_account_has_token)
      return first_gaia_account;
    // As a last resort, use the first Chrome account.
    return chrome_accounts.empty() ? CoreAccountId() : chrome_accounts[0];
  }

  // While Chrome is running, try the first Gaia account, and then the
  // primary account.
  if (first_gaia_account_has_token)
    return first_gaia_account;
  if (primary_account_has_token)
    return primary_account;

  // Changing the first Gaia account while Chrome is running would be
  // confusing for the user. Logout everything.
  return CoreAccountId();
}

gaia::MultiloginMode DiceAccountReconcilorDelegate::CalculateModeForReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const CoreAccountId& primary_account,
    bool first_execution,
    bool primary_has_error) const {
  const bool sync_enabled = !primary_account.empty();
  const bool first_gaia_is_primary =
      !gaia_accounts.empty() && (gaia_accounts[0].id == primary_account);
  return sync_enabled && first_execution && !primary_has_error &&
                 !first_gaia_is_primary
             ? gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER
             : gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER;
}

std::vector<CoreAccountId>
DiceAccountReconcilorDelegate::GetChromeAccountsForReconcile(
    const std::vector<CoreAccountId>& chrome_accounts,
    const CoreAccountId& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const gaia::MultiloginMode mode) const {
  if (mode == gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER) {
    return ReorderChromeAccountsForReconcile(chrome_accounts, primary_account,
                                             gaia_accounts);
  }
  if (gaia_accounts.empty() &&
      base::Contains(chrome_accounts, last_known_first_account_)) {
    // In PRESERVE mode in case accounts in cookies are accidentally lost we
    // should put cached first account first since Gaia has no information about
    // it.
    return ReorderChromeAccountsForReconcile(
        chrome_accounts, last_known_first_account_, gaia_accounts);
  }
  return chrome_accounts;
}

AccountReconcilorDelegate::RevokeTokenOption
DiceAccountReconcilorDelegate::ShouldRevokeSecondaryTokensBeforeReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  // During the Dice migration step, before Dice is actually enabled, chrome
  // tokens must be cleared when the cookies are cleared.
  if ((account_consistency_ == AccountConsistencyMethod::kDiceMigration) &&
      gaia_accounts.empty()) {
    return RevokeTokenOption::kRevoke;
  }

  return (account_consistency_ == AccountConsistencyMethod::kDice)
             ? RevokeTokenOption::kRevokeIfInError
             : RevokeTokenOption::kDoNotRevoke;
}

bool DiceAccountReconcilorDelegate::ShouldRevokeTokensOnCookieDeleted() {
  return account_consistency_ == AccountConsistencyMethod::kDice;
}

void DiceAccountReconcilorDelegate::OnReconcileFinished(
    const CoreAccountId& first_account,
    bool reconcile_is_noop) {
  last_known_first_account_ = first_account;

  // Migration happens on startup if the last reconcile was a no-op and the
  // refresh tokens are Dice-compatible.
  signin_client_->SetReadyForDiceMigration(
      reconcile_is_noop && signin_client_->GetPrefs()->GetBoolean(
                               prefs::kTokenServiceDiceCompatible));
}

}  // namespace signin
