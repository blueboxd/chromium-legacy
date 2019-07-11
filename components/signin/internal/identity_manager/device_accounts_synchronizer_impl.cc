// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"

#include "base/logging.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"

namespace identity {

DeviceAccountsSynchronizerImpl::DeviceAccountsSynchronizerImpl(
    OAuth2TokenServiceDelegate* token_service_delegate)
    : token_service_delegate_(token_service_delegate) {
  DCHECK(token_service_delegate_);
}

DeviceAccountsSynchronizerImpl::~DeviceAccountsSynchronizerImpl() = default;

void DeviceAccountsSynchronizerImpl::ReloadAllAccountsFromSystem() {
  token_service_delegate_->ReloadAccountsFromSystem(
      /*primary_account_id=*/CoreAccountId());
}

void DeviceAccountsSynchronizerImpl::ReloadAccountFromSystem(
    const CoreAccountId& account_id) {
  token_service_delegate_->AddAccountFromSystem(account_id);
}

}  // namespace identity
