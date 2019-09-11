// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_info.h"

#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "sandbox/win/src/sandbox_constants.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/target_process.h"

namespace sandbox {

namespace {

base::Value ProcessIdList(std::vector<uint32_t>& pids) {
  base::ListValue results;
  for (auto pid : pids) {
    results.GetList().push_back(base::Value(base::strict_cast<double>(pid)));
  }

  return std::move(results);
}
}  // namespace

// We are a friend of PolicyBase so that we can steal its private members
// quickly in the BrokerServices tracker thread.
PolicyInfo::PolicyInfo(PolicyBase* policy) {
  DCHECK(policy);
  // TODO(crbug/997273) Add more fields once webui plumbing is complete.
  {
    AutoLock lock(&policy->lock_);
    for (auto&& target_process : policy->targets_) {
      process_ids_.push_back(
          base::strict_cast<uint32_t>(target_process->ProcessId()));
    }
  }
}

PolicyInfo::~PolicyInfo() {}

base::Value PolicyInfo::GetValue() {
  // TODO(crbug/997273) Add more fields once webui plumbing is complete.
  base::DictionaryValue val;
  val.SetKey(kProcessIds, ProcessIdList(process_ids_));
  return std::move(val);
}

}  // namespace sandbox
