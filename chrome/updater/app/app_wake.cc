// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_wake.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/update_apps.h"
#include "chrome/updater/update_service.h"

namespace updater {

class AppWake : public App {
 public:
  AppWake() = default;

 private:
  ~AppWake() override = default;

  // Overrides for App.
  void FirstTaskRun() override;
  void Initialize() override;
  void Uninitialize() override;

  scoped_refptr<Configurator> config_;
  scoped_refptr<UpdateService> update_service_;
};

void AppWake::Initialize() {
  config_ = base::MakeRefCounted<Configurator>();
}

void AppWake::Uninitialize() {
  update_service_->Uninitialize();
}

// AppWake triggers an update of all registered applications.
void AppWake::FirstTaskRun() {
  update_service_ = CreateUpdateService(config_);
  update_service_->UpdateAll(
      base::BindRepeating([](UpdateService::UpdateState) {}),
      base::BindOnce(
          [](base::OnceCallback<void(int)> quit, UpdateService::Result result) {
            const int exit_code = static_cast<int>(result);
            VLOG(0) << "UpdateAll complete: exit_code = " << exit_code;
            std::move(quit).Run(exit_code);
          },
          base::BindOnce(&AppWake::Shutdown, this)));
}

scoped_refptr<App> AppWakeInstance() {
  return AppInstance<AppWake>();
}

}  // namespace updater
