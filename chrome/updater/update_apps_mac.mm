// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/control_service_out_of_process.h"
#include "chrome/updater/mac/update_service_out_of_process.h"
#include "chrome/updater/service_scope.h"
#include "chrome/updater/update_service_in_process.h"

namespace updater {

scoped_refptr<UpdateService> CreateUpdateService(
    scoped_refptr<update_client::Configurator> config) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kSingleProcessSwitch))
    return base::MakeRefCounted<UpdateServiceInProcess>(config);

  return cmdline->HasSwitch(kSystemSwitch)
             ? base::MakeRefCounted<UpdateServiceOutOfProcess>(
                   ServiceScope::kSystem)
             : base::MakeRefCounted<UpdateServiceOutOfProcess>(
                   ServiceScope::kUser);
}

scoped_refptr<ControlService> CreateControlService() {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(kSystemSwitch)
             ? base::MakeRefCounted<ControlServiceOutOfProcess>(
                   ServiceScope::kSystem)
             : base::MakeRefCounted<ControlServiceOutOfProcess>(
                   ServiceScope::kUser);
}

}  // namespace updater
