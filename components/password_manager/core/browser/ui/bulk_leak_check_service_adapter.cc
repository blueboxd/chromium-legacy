// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"

#include <memory>
#include <tuple>

#include "base/logging.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace password_manager {

namespace {

using autofill::PasswordForm;

// Simple struct that stores a canonicalized credential.
struct CanonicalizedCredential {
  explicit CanonicalizedCredential(const PasswordForm& form)
      : canonicalized_username(CanonicalizeUsername(form.username_value)),
        password(form.password_value) {}

  base::string16 canonicalized_username;
  base::string16 password;
};

bool operator<(const CanonicalizedCredential& lhs,
               const CanonicalizedCredential& rhs) {
  return std::tie(lhs.canonicalized_username, lhs.password) <
         std::tie(rhs.canonicalized_username, rhs.password);
}

}  // namespace

const char kBulkLeakCheckDataKey[] = "bulk-leak-check-data";

BulkLeakCheckData::BulkLeakCheckData(const PasswordForm& leaked_form)
    : leaked_forms({leaked_form}) {}

BulkLeakCheckData::BulkLeakCheckData(std::vector<PasswordForm> leaked_forms)
    : leaked_forms(std::move(leaked_forms)) {}

BulkLeakCheckData::~BulkLeakCheckData() = default;

BulkLeakCheckServiceAdapter::BulkLeakCheckServiceAdapter(
    SavedPasswordsPresenter* presenter,
    BulkLeakCheckService* service)
    : presenter_(presenter), service_(service) {
  DCHECK(presenter_);
  DCHECK(service_);
  presenter_->AddObserver(this);
}

BulkLeakCheckServiceAdapter::~BulkLeakCheckServiceAdapter() {
  presenter_->RemoveObserver(this);
}

bool BulkLeakCheckServiceAdapter::StartBulkLeakCheck() {
  if (service_->state() == BulkLeakCheckService::State::kRunning)
    return false;

  // Even though the BulkLeakCheckService performs canonicalization eventually
  // we do it here to de-dupe credentials that have the same canonicalized form.
  // Each canonicalized credential is mapped to a list of saved passwords that
  // correspond to this credential.
  std::map<CanonicalizedCredential, std::vector<PasswordForm>> canonicalized;
  for (const PasswordForm& form : presenter_->GetSavedPasswords())
    canonicalized[CanonicalizedCredential(form)].push_back(form);

  // Build the list of LeakCheckCredentials and attach the corresponding saved
  // passwords as UserData. Lastly,forward them to the service to start the
  // check.
  std::vector<LeakCheckCredential> credentials;
  credentials.reserve(canonicalized.size());

  for (auto& pair : canonicalized) {
    const CanonicalizedCredential& credential = pair.first;
    std::vector<PasswordForm>& forms = pair.second;
    credentials.emplace_back(credential.canonicalized_username,
                             credential.password);
    credentials.back().SetUserData(
        kBulkLeakCheckDataKey,
        std::make_unique<BulkLeakCheckData>(std::move(forms)));
  }

  service_->CheckUsernamePasswordPairs(std::move(credentials));
  return true;
}

void BulkLeakCheckServiceAdapter::StopBulkLeakCheck() {
  service_->Cancel();
}

BulkLeakCheckService::State BulkLeakCheckServiceAdapter::GetBulkLeakCheckState()
    const {
  return service_->state();
}

size_t BulkLeakCheckServiceAdapter::GetPendingChecksCount() const {
  return service_->GetPendingChecksCount();
}

void BulkLeakCheckServiceAdapter::OnEdited(const PasswordForm& form) {
  // Here no extra canonicalization is needed, as there are no other forms we
  // could de-dupe before we pass it on to the service.
  std::vector<LeakCheckCredential> credentials;
  credentials.emplace_back(form.username_value, form.password_value);
  credentials.back().SetUserData(kBulkLeakCheckDataKey,
                                 std::make_unique<BulkLeakCheckData>(form));
  service_->CheckUsernamePasswordPairs(std::move(credentials));
}

}  // namespace password_manager
