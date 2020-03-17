// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_IN_PROCESS_H_
#define CHROME_UPDATER_UPDATE_SERVICE_IN_PROCESS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace update_client {
class Configurator;
class UpdateClient;
}  // namespace update_client

namespace updater {
class PersistedData;
struct RegistrationRequest;
struct RegistrationResponse;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceInProcess : public UpdateService {
 public:
  explicit UpdateServiceInProcess(
      scoped_refptr<update_client::Configurator> config);

  UpdateServiceInProcess(const UpdateServiceInProcess&) = delete;
  UpdateServiceInProcess& operator=(const UpdateServiceInProcess&) = delete;

  ~UpdateServiceInProcess() override;

  // Overrides for updater::UpdateService.
  // Registers given request to the updater.
  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) override;

  // Update-checks all registered applications. Calls |callback| once the
  // operation is complete.
  void UpdateAll(base::OnceCallback<void(Result)> callback) override;

  // Update-checks one registered application. Calls |state_updates| as the
  // update is processed and |done| once the operation is complete.
  void Update(const std::string& app_id,
              Priority priority,
              base::RepeatingCallback<void(UpdateState)> state_update,
              base::OnceCallback<void(Result)> done) override;

  void Uninitialize() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<update_client::Configurator> config_;
  scoped_refptr<PersistedData> persisted_data_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<update_client::UpdateClient> update_client_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_IN_PROCESS_H_
