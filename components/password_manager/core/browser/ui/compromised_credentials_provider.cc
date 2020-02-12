// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"
#include "base/logging.h"

namespace password_manager {

CompromisedCredentialsProvider::CompromisedCredentialsProvider() = default;

CompromisedCredentialsProvider::~CompromisedCredentialsProvider() = default;

CompromisedCredentialsProvider::CredentialsView
CompromisedCredentialsProvider::GetCompromisedCredentials() const {
  // TODO(crbug.com/1047726): Implement.
  NOTIMPLEMENTED();
  return {};
}

void CompromisedCredentialsProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompromisedCredentialsProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CompromisedCredentialsProvider::NotifyCompromisedCredentialsChanged(
    CredentialsView credentials) {
  for (auto& observer : observers_)
    observer.OnCompromisedCredentialsChanged(credentials);
}

}  // namespace password_manager
