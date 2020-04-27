// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/linux/bpf_network_policy_linux.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_seccomp_bpf_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace service_manager {

NetworkProcessPolicy::NetworkProcessPolicy() {}

NetworkProcessPolicy::~NetworkProcessPolicy() {}

ResultExpr NetworkProcessPolicy::EvaluateSyscall(int sysno) const {
  auto* broker_process = SandboxLinux::GetInstance()->broker_process();
  if (broker_process->IsSyscallAllowed(sysno)) {
    return Trap(BrokerProcess::SIGSYS_Handler, broker_process);
  }

  // TODO(tsepez): FIX this.
  return Allow();
}

}  // namespace service_manager
