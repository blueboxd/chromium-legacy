// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_command_line.h"

#include "base/json/json_reader.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

PolicyLoaderCommandLine::PolicyLoaderCommandLine(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::CommandLine& command_line)
    : AsyncPolicyLoader(task_runner), command_line_(command_line) {}
PolicyLoaderCommandLine::~PolicyLoaderCommandLine() = default;

void PolicyLoaderCommandLine::InitOnBackgroundThread() {}

std::unique_ptr<PolicyBundle> PolicyLoaderCommandLine::Load() {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  if (!command_line_.HasSwitch(switches::kChromePolicy))
    return bundle;

  base::Optional<base::Value> policies = base::JSONReader::Read(
      command_line_.GetSwitchValueASCII(switches::kChromePolicy));

  if (!policies || !policies->is_dict())
    return bundle;

  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .LoadFrom(&base::Value::AsDictionaryValue(*policies),
                POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_COMMAND_LINE);

  return bundle;
}

}  // namespace policy
