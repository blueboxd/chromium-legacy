// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_sandbox_win.h"

#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_types.h"

// NOTE: changes to this code need to be reviewed by the security team.
namespace network {

// Right now, this policy is essentially unsandboxed, but with default process
// mitigations applied. This will be tighted up in future releases.
bool NetworkPreSpawnTarget(sandbox::TargetPolicy* policy,
                           const base::CommandLine& cmd_line) {
  sandbox::ResultCode result = policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                                                     sandbox::USER_UNPROTECTED);
  if (result != sandbox::ResultCode::SBOX_ALL_OK)
    return false;
  result = sandbox::policy::SandboxWin::SetJobLevel(
      cmd_line, sandbox::JOB_UNPROTECTED, 0, policy);
  if (result != sandbox::ResultCode::SBOX_ALL_OK)
    return false;
  return true;
}

}  // namespace network
