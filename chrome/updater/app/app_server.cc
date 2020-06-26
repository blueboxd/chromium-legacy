// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server.h"

#include <memory>

#include "base/bind.h"
#include "base/version.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_version.h"
#include "components/prefs/pref_service.h"

namespace updater {

AppServer::AppServer() = default;

AppServer::~AppServer() = default;

void AppServer::Initialize() {
  first_task_ = ModeCheck();
}

base::OnceClosure AppServer::ModeCheck() {
  std::unique_ptr<UpdaterPrefs> local_prefs = CreateLocalPrefs();
  if (!local_prefs->GetPrefService()->GetBoolean(kPrefQualified))
    return base::BindOnce(&AppServer::Qualify, this, std::move(local_prefs));

  std::unique_ptr<UpdaterPrefs> global_prefs = CreateGlobalPrefs();
  if (!global_prefs) {
    return base::BindOnce(&AppServer::Shutdown, this,
                          kErrorFailedToLockPrefsMutex);
  }

  base::Version this_version(UPDATER_VERSION_STRING);
  base::Version active_version(
      global_prefs->GetPrefService()->GetString(kPrefActiveVersion));
  if (this_version < active_version)
    return base::BindOnce(&AppServer::UninstallSelf, this);

  if (this_version > active_version ||
      global_prefs->GetPrefService()->GetBoolean(kPrefSwapping)) {
    if (!SwapVersions(global_prefs->GetPrefService()))
      return base::BindOnce(&AppServer::Shutdown, this, kErrorFailedToSwap);
  }

  config_ = base::MakeRefCounted<Configurator>(std::move(global_prefs));
  return base::BindOnce(&AppServer::ActiveDuty, this);
}

void AppServer::Uninitialize() {
  if (config_)
    PrefsCommitPendingWrites(config_->GetPrefService());
}

void AppServer::FirstTaskRun() {
  std::move(first_task_).Run();
}

void AppServer::Qualify(std::unique_ptr<UpdaterPrefs> local_prefs) {
  // For now, assume qualification succeeds.
  local_prefs->GetPrefService()->SetBoolean(kPrefQualified, true);
  PrefsCommitPendingWrites(local_prefs->GetPrefService());
  Shutdown(kErrorOk);
}

bool AppServer::SwapVersions(PrefService* global_prefs) {
  global_prefs->SetBoolean(kPrefSwapping, true);
  PrefsCommitPendingWrites(global_prefs);
  bool result = SwapRPCInterfaces();
  if (!result)
    return false;
  global_prefs->SetString(kPrefActiveVersion, UPDATER_VERSION_STRING);
  global_prefs->SetBoolean(kPrefSwapping, false);
  PrefsCommitPendingWrites(global_prefs);
  return true;
}

}  // namespace updater
