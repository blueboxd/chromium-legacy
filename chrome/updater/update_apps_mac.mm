// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_apps.h"

#include "chrome/updater/mac/update_service_out_of_process.h"

namespace updater {

std::unique_ptr<UpdateService> CreateUpdateService() {
  return std::make_unique<UpdateServiceOutOfProcess>();
}

}  // namespace updater
