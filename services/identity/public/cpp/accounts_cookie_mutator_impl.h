// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"

class GaiaCookieManagerService;

namespace gaia {
enum class GaiaSource;
}

namespace identity {

// Concrete implementation of the AccountsCookieMutator interface.
class AccountsCookieMutatorImpl : public AccountsCookieMutator {
 public:
  explicit AccountsCookieMutatorImpl(
      GaiaCookieManagerService* gaia_cookie_manager_service,
      AccountTrackerService* account_tracker_service);
  ~AccountsCookieMutatorImpl() override;

  void AddAccountToCookie(
      const CoreAccountId& account_id,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback) override;

  void AddAccountToCookieWithToken(
      const CoreAccountId& account_id,
      const std::string& access_token,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback) override;

  void SetAccountsInCookie(
      const std::vector<CoreAccountId>& account_ids,
      gaia::GaiaSource source,
      base::OnceCallback<void(signin::SetAccountsInCookieResult)>
          set_accounts_in_cookies_completed_callback) override;

  void TriggerCookieJarUpdate() override;

  void LogOutAllAccounts(gaia::GaiaSource source) override;

 private:
  GaiaCookieManagerService* gaia_cookie_manager_service_;
  AccountTrackerService* account_tracker_service_;

  DISALLOW_COPY_AND_ASSIGN(AccountsCookieMutatorImpl);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
