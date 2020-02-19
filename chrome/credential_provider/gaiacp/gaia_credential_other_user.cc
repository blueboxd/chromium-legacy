// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential_other_user.h"

#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

COtherUserGaiaCredential::COtherUserGaiaCredential() = default;

COtherUserGaiaCredential::~COtherUserGaiaCredential() = default;

HRESULT COtherUserGaiaCredential::FinalConstruct() {
  LOGFN(VERBOSE);
  return S_OK;
}

void COtherUserGaiaCredential::FinalRelease() {
  LOGFN(VERBOSE);
}

}  // namespace credential_provider
