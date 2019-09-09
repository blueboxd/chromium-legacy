// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_form_fetcher.h"
#include "base/logging.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

namespace password_manager {

MutliStoreFormFetcher::MutliStoreFormFetcher() = default;

MutliStoreFormFetcher::~MutliStoreFormFetcher() = default;

void MutliStoreFormFetcher::AddConsumer(FormFetcher::Consumer* consumer) {
  NOTIMPLEMENTED();
}

void MutliStoreFormFetcher::RemoveConsumer(FormFetcher::Consumer* consumer) {
  NOTIMPLEMENTED();
}

FormFetcher::State MutliStoreFormFetcher::GetState() const {
  NOTIMPLEMENTED();
  return FormFetcher::State::WAITING;
}

const std::vector<InteractionsStats>&
MutliStoreFormFetcher::GetInteractionsStats() const {
  NOTIMPLEMENTED();
  return interactions_stats_;
}

std::vector<const PasswordForm*> MutliStoreFormFetcher::GetNonFederatedMatches()
    const {
  NOTIMPLEMENTED();
  return std::vector<const PasswordForm*>();
}

std::vector<const PasswordForm*> MutliStoreFormFetcher::GetFederatedMatches()
    const {
  NOTIMPLEMENTED();
  return std::vector<const PasswordForm*>();
}

std::vector<const PasswordForm*> MutliStoreFormFetcher::GetBlacklistedMatches()
    const {
  NOTIMPLEMENTED();
  return std::vector<const PasswordForm*>();
}

const std::vector<const PasswordForm*>&
MutliStoreFormFetcher::GetAllRelevantMatches() const {
  NOTIMPLEMENTED();
  return non_federated_same_scheme_;
}

const std::map<base::string16, const PasswordForm*>&
MutliStoreFormFetcher::GetBestMatches() const {
  NOTIMPLEMENTED();
  return best_matches_;
}

const PasswordForm* MutliStoreFormFetcher::GetPreferredMatch() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void MutliStoreFormFetcher::Fetch() {
  NOTIMPLEMENTED();
}

std::unique_ptr<FormFetcher> MutliStoreFormFetcher::Clone() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace password_manager
