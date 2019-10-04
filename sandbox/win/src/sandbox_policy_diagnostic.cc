// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_diagnostic.h"

#include <stddef.h>

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "sandbox/win/src/sandbox_constants.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

base::Value ProcessIdList(std::vector<uint32_t> process_ids) {
  base::Value results(base::Value::Type::LIST);
  for (const auto pid : process_ids) {
    results.GetList().push_back(base::Value(base::strict_cast<double>(pid)));
  }
  return results;
}

std::string GetTokenLevelInEnglish(TokenLevel token) {
  switch (token) {
    case USER_LOCKDOWN:
      return "Lockdown";
    case USER_RESTRICTED:
      return "Restricted";
    case USER_LIMITED:
      return "Limited";
    case USER_INTERACTIVE:
      return "Interactive";
    case USER_NON_ADMIN:
      return "Non Admin";
    case USER_RESTRICTED_SAME_ACCESS:
      return "Restricted Same Access";
    case USER_UNPROTECTED:
      return "Unprotected";
    case USER_RESTRICTED_NON_ADMIN:
      return "Restricted Non Admin";
    case USER_LAST:
    default:
      DCHECK(false) << "Unknown TokenType";
      return "Unknown";
  }
}

std::string GetJobLevelInEnglish(JobLevel job) {
  switch (job) {
    case JOB_LOCKDOWN:
      return "Lockdown";
    case JOB_RESTRICTED:
      return "Restricted";
    case JOB_LIMITED_USER:
      return "Limited User";
    case JOB_INTERACTIVE:
      return "Interactive";
    case JOB_UNPROTECTED:
      return "Unprotected";
    case JOB_NONE:
      return "None";
    default:
      DCHECK(false) << "Unknown JobLevel";
      return "Unknown";
  }
}

std::string GetIntegrityLevelInEnglish(IntegrityLevel integrity) {
  switch (integrity) {
    case INTEGRITY_LEVEL_SYSTEM:
      return "S-1-16-16384 System";
    case INTEGRITY_LEVEL_HIGH:
      return "S-1-16-12288 High";
    case INTEGRITY_LEVEL_MEDIUM:
      return "S-1-16-8192 Medium";
    case INTEGRITY_LEVEL_MEDIUM_LOW:
      return "S-1-16-6144 Medium Low";
    case INTEGRITY_LEVEL_LOW:
      return "S-1-16-4096 Low";
    case INTEGRITY_LEVEL_BELOW_LOW:
      return "S-1-16-2048 Below Low";
    case INTEGRITY_LEVEL_UNTRUSTED:
      return "S-1-16-0 Untrusted";
    case INTEGRITY_LEVEL_LAST:
      return "Default";
    default:
      DCHECK(false) << "Unknown IntegrityLevel";
      return "Unknown";
  }
}

base::string16 GetSidAsString(const Sid* sid) {
  base::string16 result;
  if (!sid->ToSddlString(&result))
    DCHECK(false) << "Failed to make sddl string";
  return result;
}

std::string GetMitigationsAsHex(MitigationFlags mitigations) {
  return base::StringPrintf("%016" PRIx64,
                            base::checked_cast<uint64_t>(mitigations));
}

std::string GetPlatformMitigationsAsHex(MitigationFlags mitigations) {
  DWORD64 platform_flags[2] = {0};
  size_t flags_size = 0;
  sandbox::ConvertProcessMitigationsToPolicy(mitigations, &(platform_flags[0]),
                                             &flags_size);
  DCHECK(flags_size / sizeof(DWORD64) <= 2)
      << "Unexpected mitigation flags size";
  if (flags_size == 2 * sizeof(DWORD64))
    return base::StringPrintf("%016" PRIx64 "%016" PRIx64, platform_flags[0],
                              platform_flags[1]);
  return base::StringPrintf("%016" PRIx64, platform_flags[0]);
}

}  // namespace

// We are a friend of PolicyBase so that we can steal its private members
// quickly in the BrokerServices tracker thread.
PolicyDiagnostic::PolicyDiagnostic(PolicyBase* policy) {
  DCHECK(policy);
  // TODO(crbug/997273) Add more fields once webui plumbing is complete.
  {
    AutoLock lock(&policy->lock_);
    for (auto&& target_process : policy->targets_) {
      process_ids_.push_back(
          base::strict_cast<uint32_t>(target_process->ProcessId()));
    }
  }
  lockdown_level_ = policy->lockdown_level_;
  job_level_ = policy->job_level_;

  // Select the final integrity level.
  if (policy->delayed_integrity_level_ == INTEGRITY_LEVEL_LAST)
    desired_integrity_level_ = policy->integrity_level_;
  else
    desired_integrity_level_ = policy->delayed_integrity_level_;

  desired_mitigations_ = policy->mitigations_ | policy->delayed_mitigations_;

  if (policy->app_container_profile_)
    app_container_sid_ =
        std::make_unique<Sid>(policy->app_container_profile_->GetPackageSid());
  if (policy->lowbox_sid_)
    lowbox_sid_ = std::make_unique<Sid>(policy->lowbox_sid_);
}

PolicyDiagnostic::~PolicyDiagnostic() = default;

const char* PolicyDiagnostic::JsonString() {
  // Lazily constructs json_string_.
  if (json_string_)
    return json_string_->c_str();

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kProcessIds, ProcessIdList(process_ids_));
  value.SetKey(kLockdownLevel,
               base::Value(GetTokenLevelInEnglish(lockdown_level_)));
  value.SetKey(kJobLevel, base::Value(GetJobLevelInEnglish(job_level_)));
  value.SetKey(
      kDesiredIntegrityLevel,
      base::Value(GetIntegrityLevelInEnglish(desired_integrity_level_)));
  value.SetKey(kDesiredMitigations,
               base::Value(GetMitigationsAsHex(desired_mitigations_)));
  value.SetKey(kPlatformMitigations,
               base::Value(GetPlatformMitigationsAsHex(desired_mitigations_)));

  if (app_container_sid_)
    value.SetKey(kAppContainerSid,
                 base::Value(GetSidAsString(app_container_sid_.get())));

  if (lowbox_sid_)
    value.SetKey(kLowboxSid, base::Value(GetSidAsString(lowbox_sid_.get())));

  auto json_string = std::make_unique<std::string>();
  JSONStringValueSerializer to_json(json_string.get());
  CHECK(to_json.Serialize(value));
  json_string_ = std::move(json_string);
  return json_string_->c_str();
}

}  // namespace sandbox
