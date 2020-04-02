// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_in_process.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

UpdateServiceInProcess::UpdateServiceInProcess(
    scoped_refptr<update_client::Configurator> config)
    : config_(config),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      update_client_(update_client::UpdateClientFactory(config)) {}

void UpdateServiceInProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  persisted_data_->RegisterApp(request);

  // Result of registration. Currently there's no error handling in
  // PersistedData, so we assume success every time, which is why we respond
  // with 0.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), RegistrationResponse(0)));
}

namespace {

std::vector<base::Optional<update_client::CrxComponent>> GetComponents(
    scoped_refptr<PersistedData> persisted_data,
    const std::vector<std::string>& ids) {
  std::vector<base::Optional<update_client::CrxComponent>> components;
  for (const auto& id : ids) {
    components.push_back(base::MakeRefCounted<Installer>(id, persisted_data)
                             ->MakeCrxComponent());
  }
  return components;
}

void UpdateStateCallbackRun(UpdateService::StateChangeCallback state_update,
                            update_client::CrxUpdateItem crx_update_item) {
  UpdateService::UpdateState update_state =
      UpdateService::UpdateState::kUnknown;
  switch (crx_update_item.state) {
    case update_client::ComponentState::kNew:
      update_state = UpdateService::UpdateState::kNotStarted;
      break;
    case update_client::ComponentState::kChecking:
      update_state = UpdateService::UpdateState::kCheckingForUpdates;
      break;
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDownloadingDiff:
      update_state = UpdateService::UpdateState::kDownloading;
      break;
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdatingDiff:
      update_state = UpdateService::UpdateState::kInstalling;
      break;
    case update_client::ComponentState::kUpdated:
      update_state = UpdateService::UpdateState::kUpdated;
      break;
    case update_client::ComponentState::kUpToDate:
      update_state = UpdateService::UpdateState::kNoUpdate;
      break;
    case update_client::ComponentState::kUpdateError:
      update_state = UpdateService::UpdateState::kUpdateError;
      break;
    default:
      break;
  }
  state_update.Run(update_state);
}

}  // namespace

void UpdateServiceInProcess::UpdateAll(StateChangeCallback state_update,
                                       Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto app_ids = persisted_data_->GetAppIds();
  DCHECK(base::Contains(app_ids, kUpdaterAppId));

  update_client_->Update(
      app_ids, base::BindOnce(&GetComponents, persisted_data_),
      base::BindRepeating(&UpdateStateCallbackRun, state_update), false,
      std::move(callback));
}

void UpdateServiceInProcess::Update(const std::string& app_id,
                                    Priority priority,
                                    StateChangeCallback state_update,
                                    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  update_client_->Update(
      {app_id}, base::BindOnce(&GetComponents, persisted_data_),
      base::BindRepeating(&UpdateStateCallbackRun, state_update),
      priority == Priority::kForeground, std::move(callback));
}

void UpdateServiceInProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefsCommitPendingWrites(config_->GetPrefService());
}

UpdateServiceInProcess::~UpdateServiceInProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
