// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cbcm_remote_commands_factory.h"

#include "base/notreached.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

CBCMRemoteCommandsFactory::CBCMRemoteCommandsFactory() {}

CBCMRemoteCommandsFactory::~CBCMRemoteCommandsFactory() {}

std::unique_ptr<RemoteCommandJob> CBCMRemoteCommandsFactory::BuildJobForType(
    enterprise_management::RemoteCommand_Type type,
    RemoteCommandsService* service) {
  NOTREACHED();
  return nullptr;
}

}  // namespace policy
