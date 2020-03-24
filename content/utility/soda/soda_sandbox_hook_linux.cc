// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/soda/soda_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "components/soda/constants.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace soda {

namespace {

std::vector<BrokerFilePermission> GetSodaFilePermissions(
    base::FilePath latest_version_dir) {
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom")};

  // This may happen if a user doesn't have a SODA installation.
  if (!latest_version_dir.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        latest_version_dir.AsEndingWithSeparator().value()));
    permissions.push_back(
        BrokerFilePermission::ReadOnly(latest_version_dir.value()));
  }

  return permissions;
}

}  // namespace

bool SodaPreSandboxHook(service_manager::SandboxLinux::Options options) {
  void* soda_library = dlopen(GetSodaBinaryPath().value().c_str(),
                              RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  DCHECK(soda_library);

  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_READLINK,
                                   sandbox::syscall_broker::COMMAND_STAT,
                               }),
                               GetSodaFilePermissions(GetSodaDirectory()),
                               service_manager::SandboxLinux::PreSandboxHook(),
                               options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace soda
