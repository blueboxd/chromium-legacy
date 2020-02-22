// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_in_process.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/registration_data.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

UpdateServiceInProcess::UpdateServiceInProcess(
    scoped_refptr<update_client::Configurator> config)
    : config_(config),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      update_client_(update_client::UpdateClientFactory(config)) {}

void UpdateServiceInProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // For now just do the callback.
  const RegistrationResponse response;
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), response));
}

void UpdateServiceInProcess::UpdateAll(
    base::OnceCallback<void(update_client::Error)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto app_ids = Installer::FindAppIds();

  // Include the app id for the updater if it is not found. This could happen
  // before the first update for the updater has been handled. This is a
  // temporary workaround until the source of truth for the registered
  // version is resolved.
  if (!base::Contains(app_ids, kUpdaterAppId))
    app_ids.push_back(kUpdaterAppId);

  std::vector<base::Optional<update_client::CrxComponent>> components;
  for (const auto& app_id : app_ids) {
    auto installer = base::MakeRefCounted<Installer>(app_id);
    installer->FindInstallOfApp();
    components.push_back(installer->MakeCrxComponent());
  }

  update_client_->Update(
      app_ids,
      base::BindOnce(
          [](const std::vector<base::Optional<update_client::CrxComponent>>&
                 components,
             const std::vector<std::string>& ids) {
            DCHECK_EQ(components.size(), ids.size());
            return components;
          },
          components),
      false, std::move(callback));
}

UpdateServiceInProcess::~UpdateServiceInProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Block until prefs write is committed.
  base::RunLoop runloop;
  config_->GetPrefService()->CommitPendingWrite(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      runloop.QuitWhenIdleClosure()));
  runloop.Run();
}

}  // namespace updater
