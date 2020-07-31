// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/setup/setup.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_version.h"

namespace updater {

namespace {

class AppInstall : public App {
 private:
  ~AppInstall() override = default;
  void Initialize() override;
  void Uninitialize() override;
  void FirstTaskRun() override;

  void SetupDone(int result);

  scoped_refptr<Configurator> config_;
};

void AppInstall::Initialize() {
  config_ = base::MakeRefCounted<Configurator>(CreateGlobalPrefs());
}

void AppInstall::Uninitialize() {
  PrefsCommitPendingWrites(config_->GetPrefService());
}

void AppInstall::FirstTaskRun() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&InstallCandidate),
      base::BindOnce(&AppInstall::SetupDone, this));
}

void AppInstall::SetupDone(int result) {
  if (result != 0) {
    Shutdown(result);
    return;
  }

  RegistrationRequest request;
  request.app_id = kUpdaterAppId;
  request.version = base::Version(UPDATER_VERSION_STRING);

  base::MakeRefCounted<PersistedData>(config_->GetPrefService())
      ->RegisterApp(request);

  Shutdown(0);
}

}  // namespace

scoped_refptr<App> MakeAppInstall() {
  return base::MakeRefCounted<AppInstall>();
}

}  // namespace updater
