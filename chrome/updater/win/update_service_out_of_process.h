// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_
#define CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_

#include <wrl/implements.h>

#include <string>

#include "base/callback.h"
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

// There are two threads running the code in this module. The main sequence is
// bound to one thread, all the COM calls, inbound and outbound occur on the
// second thread which serializes the tasks and the invocations originating
// in the COM RPC runtime, which arrive sequentially but they are not sequenced
// through the task runner.
//
// This class implements the IUpdaterObserver interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread. However, its
// functions are invoked directly by COM RPC, and they are not sequenced through
// the thread task runner. This means that the usual mechanisms that check for
// the correct sequence are not going to work for this class.
class UpdaterObserverImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterObserver> {
 public:
  UpdaterObserverImpl(Microsoft::WRL::ComPtr<IUpdater> updater,
                      UpdateService::Callback callback);
  UpdaterObserverImpl(const UpdaterObserverImpl&) = delete;
  UpdaterObserverImpl& operator=(const UpdaterObserverImpl&) = delete;

  // Overrides for IUpdaterObserver.
  IFACEMETHODIMP OnComplete(ICompleteStatus* status) override;

 private:
  ~UpdaterObserverImpl() override;

  // Bound to the STA thread.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  UpdateService::Callback callback_;
};

// All public functions and callbacks must be called on the same sequence.
class UpdateServiceOutOfProcess : public UpdateService {
 public:
  UpdateServiceOutOfProcess();

  // Overrides for updater::UpdateService.
  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) override;
  void UpdateAll(StateChangeCallback state_update, Callback callback) override;
  void Update(const std::string& app_id,
              Priority priority,
              StateChangeCallback state_update,
              Callback callback) override;
  void Uninitialize() override;

 private:
  ~UpdateServiceOutOfProcess() override;

  // Runs on the |com_task_runner_|.
  void UpdateAllOnSTA(StateChangeCallback state_update, Callback callback);

  static void ModuleStop();

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Runs the tasks which involve outbound COM calls and inbound COM callbacks.
  // This task runner is thread-affine with the COM STA.
  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UPDATE_SERVICE_OUT_OF_PROCESS_H_
