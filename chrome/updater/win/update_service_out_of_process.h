// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_
#define CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {

using StateChangeCallback =
    base::RepeatingCallback<void(updater::UpdateService::UpdateState)>;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceOutOfProcess : public UpdateService {
 public:
  UpdateServiceOutOfProcess(const UpdateServiceOutOfProcess&) = delete;
  UpdateServiceOutOfProcess& operator=(const UpdateServiceOutOfProcess&) =
      delete;
  ~UpdateServiceOutOfProcess() override;

  static std::unique_ptr<UpdateServiceOutOfProcess> CreateInstance();

  // Overrides for updater::UpdateService.
  // Update-checks all registered applications. Calls |callback| once the
  // operation is complete.
  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) override;
  void UpdateAll(base::OnceCallback<void(Result)> callback) override;
  void Update(const std::string& app_id,
              Priority priority,
              StateChangeCallback state_update,
              base::OnceCallback<void(Result)> done) override;

 private:
  UpdateServiceOutOfProcess();
  void UpdateAllOnSTA(base::OnceCallback<void(Result)> callback);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_
