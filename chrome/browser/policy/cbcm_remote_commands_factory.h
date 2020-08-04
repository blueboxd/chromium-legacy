// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CBCM_REMOTE_COMMANDS_FACTORY_H_
#define CHROME_BROWSER_POLICY_CBCM_REMOTE_COMMANDS_FACTORY_H_

#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

namespace policy {

class CBCMRemoteCommandsFactory : public RemoteCommandsFactory {
 public:
  CBCMRemoteCommandsFactory();
  ~CBCMRemoteCommandsFactory() override;

  // RemoteCommandsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CBCM_REMOTE_COMMANDS_FACTORY_H_
