// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/soda/soda_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/component_updater/component_updater_paths.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace soda {

namespace {

constexpr base::FilePath::CharType kSodaDirName[] = FILE_PATH_LITERAL("SODA/");
constexpr base::FilePath::CharType kSodaBinaryFileName[] =
    FILE_PATH_LITERAL("SODAFiles/libsoda.so");

std::vector<BrokerFilePermission> GetSodaFilePermissions(
    base::FilePath latest_version_dir) {
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnlyRecursive(
          latest_version_dir.AsEndingWithSeparator().value())};

  return permissions;
}

}  // namespace

bool SodaPreSandboxHook(service_manager::SandboxLinux::Options options) {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);

  // Get the directory containing the latest version of SODA. In most cases
  // there will only be one version of SODA, but it is possible for there to be
  // multiple versions if a newer version of SODA was recently downloaded before
  // the old version gets cleaned up.
  base::FileEnumerator enumerator(components_dir.Append(kSodaDirName), false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath latest_version_dir;
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }

  void* soda_library =
      latest_version_dir.empty()
          ? nullptr
          : dlopen(
                latest_version_dir.Append(kSodaBinaryFileName).value().c_str(),
                RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  DCHECK(soda_library);

  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_READLINK,
                                   sandbox::syscall_broker::COMMAND_STAT,
                               }),
                               GetSodaFilePermissions(latest_version_dir),
                               service_manager::SandboxLinux::PreSandboxHook(),
                               options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace soda
