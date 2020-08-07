// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_sandbox_delegate.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#include "services/audio/audio_sandbox_win.h"

namespace content {
namespace {
// Right now, this policy is essentially unsandboxed, but with default process
// mitigations applied.
// TODO(https://crbug.com/841001) This will be tighted up in future releases.
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
}  // namespace

bool UtilitySandboxedProcessLauncherDelegate::GetAppContainerId(
    std::string* appcontainer_id) {
  if (sandbox_type_ == sandbox::policy::SandboxType::kXrCompositing &&
      base::FeatureList::IsEnabled(sandbox::policy::features::kXRSandbox)) {
    *appcontainer_id = base::WideToUTF8(cmd_line_.GetProgram().value());
    return true;
  }
  return false;
}

bool UtilitySandboxedProcessLauncherDelegate::DisableDefaultPolicy() {
  switch (sandbox_type_) {
    case sandbox::policy::SandboxType::kAudio:
      // Default policy is disabled for audio process to allow audio drivers
      // to read device properties (https://crbug.com/883326).
      return true;
    case sandbox::policy::SandboxType::kNetwork:
      // Default policy is disabled for network process to allow incremental
      // sandbox mitigations to be applied via experiments.
      return true;
    case sandbox::policy::SandboxType::kXrCompositing:
      return base::FeatureList::IsEnabled(
          sandbox::policy::features::kXRSandbox);
    default:
      return false;
  }
}

bool UtilitySandboxedProcessLauncherDelegate::ShouldLaunchElevated() {
  return sandbox_type_ ==
         sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges;
}

bool UtilitySandboxedProcessLauncherDelegate::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  if (sandbox_type_ == sandbox::policy::SandboxType::kNetwork)
    return NetworkPreSpawnTarget(policy, cmd_line_);

  if (sandbox_type_ == sandbox::policy::SandboxType::kAudio)
    return audio::AudioPreSpawnTarget(policy);

  if (sandbox_type_ == sandbox::policy::SandboxType::kProxyResolver) {
    sandbox::MitigationFlags flags = policy->GetDelayedProcessMitigations();
    flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    if (sandbox::SBOX_ALL_OK != policy->SetDelayedProcessMitigations(flags))
      return false;
    return true;
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kSpeechRecognition) {
    policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                          sandbox::USER_LIMITED);
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kIconReader) {
    policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                          sandbox::USER_LOCKDOWN);
    policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
    policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    policy->SetLockdownDefaultDacl();
    policy->SetAlternateDesktop(true);

    sandbox::MitigationFlags flags = policy->GetDelayedProcessMitigations();
    flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    if (sandbox::SBOX_ALL_OK != policy->SetDelayedProcessMitigations(flags))
      return false;

    // Allow file read. These should match IconLoader::GroupForFilepath().
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    L"\\??\\*.exe");
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    L"\\??\\*.dll");
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    L"\\??\\*.ico");
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kXrCompositing &&
      base::FeatureList::IsEnabled(sandbox::policy::features::kXRSandbox)) {
    // There were issues with some mitigations, causing an inability
    // to load OpenVR and Oculus APIs.
    // TODO(https://crbug.com/881919): Try to harden the XR Compositor
    // sandbox to use mitigations and restrict the token.
    policy->SetProcessMitigations(0);
    policy->SetDelayedProcessMitigations(0);

    std::string appcontainer_id;
    if (!GetAppContainerId(&appcontainer_id)) {
      return false;
    }
    sandbox::ResultCode result =
        sandbox::policy::SandboxWin::AddAppContainerProfileToPolicy(
            cmd_line_, sandbox_type_, appcontainer_id, policy);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }

    // Unprotected token/job.
    policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
    sandbox::policy::SandboxWin::SetJobLevel(
        cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kSharingService) {
    auto result =
        sandbox::policy::SandboxWin::AddWin32kLockdownPolicy(policy, false);
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    auto delayed_flags = policy->GetDelayedProcessMitigations();
    delayed_flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    result = policy->SetDelayedProcessMitigations(delayed_flags);
    if (result != sandbox::SBOX_ALL_OK)
      return false;
  }

  return true;
}

}  // namespace content
