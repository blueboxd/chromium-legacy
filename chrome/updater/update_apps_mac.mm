// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_apps.h"

#include "base/memory/ref_counted.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/mac/update_service_out_of_process.h"

namespace updater {

scoped_refptr<UpdateService> CreateUpdateService(
    scoped_refptr<update_client::Configurator> config) {
  return base::MakeRefCounted<UpdateServiceOutOfProcess>();
}

}  // namespace updater
