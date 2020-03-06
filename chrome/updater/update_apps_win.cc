// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_apps.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/update_service_in_process.h"

namespace updater {

std::unique_ptr<UpdateService> CreateUpdateService(
    scoped_refptr<update_client::Configurator> config) {
  // TODO(crbug.com/1048653): Try to connect to an existing OOP service. For
  // now, run an in-process service.
  return std::make_unique<UpdateServiceInProcess>(config);
}

}  // namespace updater
