// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/control_service_in_process.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_in_process.h"
#include "components/prefs/pref_service.h"

namespace updater {

ControlServiceInProcess::ControlServiceInProcess(
    scoped_refptr<updater::Configurator> config)
    : config_(config),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

void ControlServiceInProcess::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Time lastUpdateTime =
      config_->GetPrefService()->GetTime(kPrefUpdateTime);

  const base::TimeDelta timeSinceUpdate =
      base::Time::NowFromSystemTime() - lastUpdateTime;
  if (base::TimeDelta() < timeSinceUpdate &&
      timeSinceUpdate <
          base::TimeDelta::FromSeconds(config_->NextCheckDelay())) {
    VLOG(0) << "Skipping checking for updates:  "
            << timeSinceUpdate.InMinutes();
    main_task_runner_->PostTask(FROM_HERE, std::move(callback));
    return;
  }

  scoped_refptr<UpdateServiceInProcess> update_service =
      base::MakeRefCounted<UpdateServiceInProcess>(config_);

  update_service->UpdateAll(
      base::BindRepeating([](UpdateService::UpdateState) {}),
      base::BindOnce(
          [](base::OnceClosure closure,
             scoped_refptr<updater::Configurator> config,
             UpdateService::Result result) {
            const int exit_code = static_cast<int>(result);
            VLOG(0) << "UpdateAll complete: exit_code = " << exit_code;
            if (result == UpdateService::Result::kSuccess) {
              config->GetPrefService()->SetTime(
                  kPrefUpdateTime, base::Time::NowFromSystemTime());
            }
            std::move(closure).Run();
          },
          base::BindOnce(std::move(callback)), config_));
}

void ControlServiceInProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefsCommitPendingWrites(config_->GetPrefService());
}

ControlServiceInProcess::~ControlServiceInProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
