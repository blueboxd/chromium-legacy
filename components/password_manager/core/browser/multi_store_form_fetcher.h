// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/form_fetcher.h"

namespace password_manager {

// Production implementation of FormFetcher that fetches credentials associated
// with a particular origin from both the account and profile password stores.
// When adding new member fields to this class, please, update the Clone()
// method accordingly.
class MutliStoreFormFetcher : public FormFetcher {
  MutliStoreFormFetcher();
  ~MutliStoreFormFetcher() override;

  // FormFetcher:
  void AddConsumer(FormFetcher::Consumer* consumer) override;
  void RemoveConsumer(FormFetcher::Consumer* consumer) override;
  State GetState() const override;
  const std::vector<InteractionsStats>& GetInteractionsStats() const override;
  std::vector<const autofill::PasswordForm*> GetNonFederatedMatches()
      const override;
  std::vector<const autofill::PasswordForm*> GetFederatedMatches()
      const override;
  std::vector<const autofill::PasswordForm*> GetBlacklistedMatches()
      const override;
  const std::vector<const autofill::PasswordForm*>& GetAllRelevantMatches()
      const override;
  const std::map<base::string16, const autofill::PasswordForm*>&
  GetBestMatches() const override;
  const autofill::PasswordForm* GetPreferredMatch() const override;
  void Fetch() override;
  std::unique_ptr<FormFetcher> Clone() override;

 private:
  // Set of nonblacklisted PasswordForms from the password store that best match
  // the form being managed by |this|, indexed by username.
  std::map<base::string16, const autofill::PasswordForm*> best_matches_;

  // Non-federated credentials of the same scheme as the observed form.
  std::vector<const autofill::PasswordForm*> non_federated_same_scheme_;

  // Statistics for the current domain.
  std::vector<InteractionsStats> interactions_stats_;

  DISALLOW_COPY_AND_ASSIGN(MutliStoreFormFetcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
