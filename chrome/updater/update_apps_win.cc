// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_apps.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service_in_process.h"
#include "chrome/updater/win/update_service_out_of_process.h"

namespace updater {

scoped_refptr<UpdateService> CreateUpdateService(
    scoped_refptr<update_client::Configurator> config) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSingleProcessSwitch))
    return base::MakeRefCounted<UpdateServiceInProcess>(config);
  else
    return base::MakeRefCounted<UpdateServiceOutOfProcess>();
}

}  // namespace updater
