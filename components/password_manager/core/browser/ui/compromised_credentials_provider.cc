// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"
#include <iterator>

#include "base/logging.h"

namespace password_manager {

CompromisedCredentialsProvider::CompromisedCredentialsProvider(
    scoped_refptr<PasswordStore> store)
    : store_(std::move(store)) {
  DCHECK(store_);
  store_->AddDatabaseCompromisedCredentialsObserver(this);
}

CompromisedCredentialsProvider::~CompromisedCredentialsProvider() {
  store_->RemoveDatabaseCompromisedCredentialsObserver(this);
}

void CompromisedCredentialsProvider::Init() {
  store_->GetAllCompromisedCredentials(this);
}

CompromisedCredentialsProvider::CredentialsView
CompromisedCredentialsProvider::GetCompromisedCredentials() const {
  return compromised_credentials_;
}

void CompromisedCredentialsProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompromisedCredentialsProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CompromisedCredentialsProvider::OnCompromisedCredentialsChanged() {
  // Cancel ongoing requests to the password store and issue a new request.
  cancelable_task_tracker()->TryCancelAll();
  store_->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsProvider::OnGetCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  // TODO(crbug.com/1047726): Implement setting the password for each
  // credential.
  compromised_credentials_.assign(
      std::make_move_iterator(compromised_credentials.begin()),
      std::make_move_iterator(compromised_credentials.end()));
  NotifyCompromisedCredentialsChanged();
}

void CompromisedCredentialsProvider::NotifyCompromisedCredentialsChanged() {
  for (auto& observer : observers_)
    observer.OnCompromisedCredentialsChanged(compromised_credentials_);
}

}  // namespace password_manager
