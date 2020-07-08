// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_sandbox_hook_linux.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

#include "base/rand_util.h"
#include "base/system/sys_info.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace network {

bool NetworkPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  // TODO(tsepez): remove universal permission under filesytem root.
  instance->StartBrokerProcess(
      MakeBrokerCommandSet({
          sandbox::syscall_broker::COMMAND_ACCESS,
          sandbox::syscall_broker::COMMAND_MKDIR,
          sandbox::syscall_broker::COMMAND_OPEN,
          sandbox::syscall_broker::COMMAND_READLINK,
          sandbox::syscall_broker::COMMAND_RENAME,
          sandbox::syscall_broker::COMMAND_RMDIR,
          sandbox::syscall_broker::COMMAND_STAT,
          sandbox::syscall_broker::COMMAND_UNLINK,
      }),
      {BrokerFilePermission::ReadWriteCreateRecursive("/")},
      sandbox::policy::SandboxLinux::PreSandboxHook(), options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace network
