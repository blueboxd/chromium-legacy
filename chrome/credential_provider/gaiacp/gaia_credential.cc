// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential.h"

#include "base/command_line.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"

namespace credential_provider {

CGaiaCredential::CGaiaCredential() = default;

CGaiaCredential::~CGaiaCredential() = default;

HRESULT CGaiaCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CGaiaCredential::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CGaiaCredential::GetUserGlsCommandline(
    base::CommandLine* command_line) {
  // Don't show tos when GEM isn't enabled.
  if (IsGemEnabled()) {
    // In default add user flow, the user has to accept tos
    // every time. So we need to set the show_tos switch to 1.
    command_line->AppendSwitchASCII(kShowTosSwitch, "1");
  }

  return S_OK;
}

}  // namespace credential_provider
