// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_reader.h"

namespace password_manager {
CompromisedCredentialsReader::CompromisedCredentialsReader(PasswordStore* store)
    : store_(store) {
  DCHECK(store_);
  observed_password_store_.Add(store_);
}

CompromisedCredentialsReader::~CompromisedCredentialsReader() = default;

void CompromisedCredentialsReader::Init() {
  store_->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsReader::OnCompromisedCredentialsChanged() {
  // Cancel ongoing requests to the password store and issue a new request.
  cancelable_task_tracker()->TryCancelAll();
  store_->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsReader::OnGetCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  for (auto& observer : observers_)
    observer.OnCompromisedCredentialsChanged(compromised_credentials);
}

void CompromisedCredentialsReader::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompromisedCredentialsReader::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace password_manager
