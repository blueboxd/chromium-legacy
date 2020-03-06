// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_update_all.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/update_apps.h"
#include "chrome/updater/update_service.h"

namespace updater {

class AppUpdateAll : public App {
 private:
  ~AppUpdateAll() override = default;

  // Overrides for App.
  void FirstTaskRun() override;
  void Initialize() override;

  scoped_refptr<Configurator> config_;
};

void AppUpdateAll::Initialize() {
  config_ = base::MakeRefCounted<Configurator>();
}

// AppUpdateAll triggers an update of all registered applications.
void AppUpdateAll::FirstTaskRun() {
  CreateUpdateService(config_)->UpdateAll(base::BindOnce(
      [](base::OnceCallback<void(int)> quit, update_client::Error error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        std::move(quit).Run(static_cast<int>(error));
      },
      base::BindOnce(&AppUpdateAll::Shutdown, this)));
}

scoped_refptr<App> MakeAppUpdateAll() {
  return base::MakeRefCounted<AppUpdateAll>();
}

}  // namespace updater
