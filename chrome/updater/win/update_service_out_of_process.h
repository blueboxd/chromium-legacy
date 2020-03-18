// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_
#define CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_

#include <wrl/implements.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/server/win/updater_idl.h"
#include "chrome/updater/update_service.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {

// This class implements the IUpdaterObserver interface and exposes it as a COM
// object.
class UpdaterObserverImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterObserver> {
 public:
  UpdaterObserverImpl() = default;
  UpdaterObserverImpl(const UpdaterObserverImpl&) = delete;
  UpdaterObserverImpl& operator=(const UpdaterObserverImpl&) = delete;

  // Overrides for IUpdaterObserver.
  IFACEMETHODIMP OnComplete(ICompleteStatus* status) override;

 private:
  ~UpdaterObserverImpl() override = default;
};

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
  static void ModuleStop();

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
  void Uninitialize() override;

 private:
  UpdateServiceOutOfProcess();
  void UpdateAllOnSTA(base::OnceCallback<void(Result)> callback);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_
