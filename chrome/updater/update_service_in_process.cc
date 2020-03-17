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

void UpdateServiceInProcess::UpdateAll(
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto app_ids = persisted_data_->GetAppIds();
  DCHECK(base::Contains(app_ids, kUpdaterAppId));

  update_client_->Update(
      app_ids,
      base::BindOnce(
          [](scoped_refptr<PersistedData> persisted_data,
             const std::vector<std::string>& ids) {
            std::vector<base::Optional<update_client::CrxComponent>> components;
            for (const auto& id : ids) {
              components.push_back(
                  base::MakeRefCounted<Installer>(id, persisted_data)
                      ->MakeCrxComponent());
            }
            return components;
          },
          persisted_data_),
      {}, false, std::move(callback));
}

void UpdateServiceInProcess::Update(
    const std::string& app_id,
    Priority priority,
    base::RepeatingCallback<void(UpdateState)> state_update,
    base::OnceCallback<void(Result)> done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1059020): Implement.
  NOTREACHED();
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
