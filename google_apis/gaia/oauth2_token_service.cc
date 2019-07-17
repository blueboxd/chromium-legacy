// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_token_service.h"

#include "google_apis/gaia/oauth2_token_service_delegate.h"

OAuth2TokenService::OAuth2TokenService(
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

OAuth2TokenService::~OAuth2TokenService() = default;

OAuth2TokenServiceDelegate* OAuth2TokenService::GetDelegate() {
  return delegate_.get();
}

const OAuth2TokenServiceDelegate* OAuth2TokenService::GetDelegate() const {
  return delegate_.get();
}
