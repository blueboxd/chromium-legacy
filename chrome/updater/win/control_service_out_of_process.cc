// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro is used in <wrl/module.h>. Since only the COM functionality is
// used here (while WinRT is not being used), define this macro to optimize
// compilation of <wrl/module.h> for COM-only.
#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif  // __WRL_CLASSIC_COM_STRICT__

#include "chrome/updater/win/control_service_out_of_process.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/win/constants.h"

namespace updater {
namespace {

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// This class implements the IUpdaterObserver interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread. However, its
// functions are invoked directly by COM RPC, and they are not sequenced through
// the thread task runner. This means that sequence checkers can't be used in
// this class.
class UpdaterControlObserver
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterObserver> {
 public:
  UpdaterControlObserver(
      Microsoft::WRL::ComPtr<IUpdaterControl> updater_control,
      base::OnceClosure callback)
      : com_task_runner_(base::SequencedTaskRunnerHandle::Get()),
        updater_control_(updater_control),
        callback_(std::move(callback)) {}

  UpdaterControlObserver(const UpdaterControlObserver&) = delete;
  UpdaterControlObserver& operator=(const UpdaterControlObserver&) = delete;

  // Overrides for IUpdaterObserver.
  IFACEMETHODIMP OnComplete(ICompleteStatus* status) override;

 private:
  ~UpdaterControlObserver() override = default;

  // Bound to the STA thread.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdaterControl> updater_control_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  base::OnceClosure callback_;
};

HRESULT UpdaterControlObserver::OnComplete(ICompleteStatus* status) {
  DCHECK(status);

  LONG code = 0;
  base::win::ScopedBstr message;
  CHECK(SUCCEEDED(status->get_statusCode(&code)));
  CHECK(SUCCEEDED(status->get_statusMessage(message.Receive())));

  VLOG(2) << "UpdaterObserver::OnComplete(" << code << ", " << message.Get()
          << ")";

  com_task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback_)));

  updater_control_ = nullptr;
  return S_OK;
}

}  // namespace

ControlServiceOutOfProcess::ControlServiceOutOfProcess(ServiceScope /*scope*/)
    : com_task_runner_(
          base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits)) {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      &ControlServiceOutOfProcess::ModuleStop);
}

ControlServiceOutOfProcess::~ControlServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ControlServiceOutOfProcess::ModuleStop() {
  VLOG(2) << __func__ << ": COM client is shutting down.";
}

void ControlServiceOutOfProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ControlServiceOutOfProcess::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reposts the call to the COM task runner. Adapts |callback| so that
  // the callback runs on the main sequence.
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ControlServiceOutOfProcess::RunOnSTA, this,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 base::OnceClosure callback) {
                taskrunner->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback)));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ControlServiceOutOfProcess::RunOnSTA(base::OnceClosure callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_UpdaterControlServiceClass, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    VLOG(2) << "Failed to instantiate the updater control server. " << std::hex
            << hr;
    std::move(callback).Run();
    return;
  }

  Microsoft::WRL::ComPtr<IUpdaterControl> updater_control;
  hr = server.As(&updater_control);
  if (FAILED(hr)) {
    VLOG(2) << "Failed to query the updater_control interface. " << std::hex
            << hr;
    std::move(callback).Run();
    return;
  }

  // The COM RPC takes ownership of the |observer| and owns a reference to
  // the updater object as well. As long as the |observer| retains this
  // reference to the updater control object, then the object is going to stay
  // alive.
  // The |observer| can drop its reference to the updater control object after
  // handling the last server callback, then the object model is torn down, and
  // finally, the execution flow returns back into the App object once the
  // completion callback is posted.
  auto observer = Microsoft::WRL::Make<UpdaterControlObserver>(
      updater_control, std::move(callback));
  hr = updater_control->Run(observer.Get());
  if (FAILED(hr)) {
    VLOG(2) << "Failed to call IUpdaterControl::Run" << std::hex << hr;
    std::move(callback).Run();
    return;
  }
}

}  // namespace updater
