// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_diagnostic.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
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
}

PolicyDiagnostic::~PolicyDiagnostic() = default;

const char* PolicyDiagnostic::JsonString() {
  // Lazily constructs json_string_.
  if (json_string_)
    return json_string_->c_str();

  auto json_string = std::make_unique<std::string>();
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kProcessIds, ProcessIdList(process_ids_));
  JSONStringValueSerializer to_json(json_string.get());
  CHECK(to_json.Serialize(value));
  json_string_ = std::move(json_string);
  return json_string_->c_str();
}

}  // namespace sandbox
