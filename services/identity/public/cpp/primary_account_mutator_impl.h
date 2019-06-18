// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_

#include "services/identity/public/cpp/primary_account_mutator.h"

#include <string>

class AccountTrackerService;
class PrefService;
class PrimaryAccountManager;

namespace identity {

// Concrete implementation of PrimaryAccountMutator that is based on the
// PrimaryAccountManager API.
class PrimaryAccountMutatorImpl : public PrimaryAccountMutator {
 public:
  PrimaryAccountMutatorImpl(AccountTrackerService* account_tracker,
                            PrimaryAccountManager* primary_account_manager,
                            PrefService* pref_service);
  ~PrimaryAccountMutatorImpl() override;

  // PrimaryAccountMutator implementation.
#if !defined(OS_CHROMEOS)
  bool SetPrimaryAccount(const std::string& account_id) override;
  bool ClearPrimaryAccount(
      ClearAccountsAction action,
      signin_metrics::ProfileSignout source_metric,
      signin_metrics::SignoutDelete delete_metric) override;
#else
  bool SetPrimaryAccountAndUpdateAccountInfo(const std::string& gaia_id,
                                             const std::string& email) override;
#endif

 private:
  // Pointers to the services used by the PrimaryAccountMutatorImpl. They
  // *must* outlive this instance.
  AccountTrackerService* account_tracker_ = nullptr;
  PrimaryAccountManager* primary_account_manager_ = nullptr;
  PrefService* pref_service_ = nullptr;
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
