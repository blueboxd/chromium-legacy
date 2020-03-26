// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro is used in <wrl/module.h>. Since only the COM functionality is
// used here (while WinRT is not being used), define this macro to optimize
// compilation of <wrl/module.h> for COM-only.
#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif  // __WRL_CLASSIC_COM_STRICT__

#include "chrome/updater/win/update_service_out_of_process.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/module.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/server/win/updater_idl.h"

namespace {

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

namespace updater {

HRESULT UpdaterObserverImpl::OnComplete(ICompleteStatus* status) {
  DCHECK(status);

  LONG code = 0;
  base::win::ScopedBstr message;
  CHECK(SUCCEEDED(status->get_statusCode(&code)));
  CHECK(SUCCEEDED(status->get_statusMessage(message.Receive())));

  VLOG(2) << "UpdaterObserverImpl::OnComplete(" << code << ", " << message.Get()
          << ")";
  return S_OK;
}

UpdateServiceOutOfProcess::UpdateServiceOutOfProcess() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      &UpdateServiceOutOfProcess::ModuleStop);
  com_task_runner_ = base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits);
}

UpdateServiceOutOfProcess::~UpdateServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpdateServiceOutOfProcess::ModuleStop() {
  VLOG(2) << "UpdateServiceOutOfProcess::ModuleStop";
}

void UpdateServiceOutOfProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sorin) the updater must be run with "--single-process" until
  // crbug.com/1053729 is resolved.
  NOTREACHED();
}

void UpdateServiceOutOfProcess::UpdateAll(
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sorin) the updater must be run with "--single-process" until
  // crbug.com/1053729 is resolved.
  com_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UpdateServiceOutOfProcess::UpdateAllOnSTA,
                                base::Unretained(this), std::move(callback)));
}

void UpdateServiceOutOfProcess::Update(const std::string& app_id,
                                       UpdateService::Priority priority,
                                       StateChangeCallback state_update,
                                       base::OnceCallback<void(Result)> done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sorin) the updater must be run with "--single-process" until
  // crbug.com/1053729 is resolved.
  NOTREACHED();
}

void UpdateServiceOutOfProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpdateServiceOutOfProcess::UpdateAllOnSTA(
    base::OnceCallback<void(Result)> callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_UpdaterClass, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    VLOG(2) << "Failed to instantiate the update server. " << std::hex << hr;
    std::move(callback).Run(static_cast<Result>(hr));
    return;
  }

  Microsoft::WRL::ComPtr<IUpdater> updater;
  hr = server.As(&updater);
  if (FAILED(hr)) {
    VLOG(2) << "Failed to query the updater interface. " << std::hex << hr;
    std::move(callback).Run(static_cast<Result>(hr));
    return;
  }

  auto observer = Microsoft::WRL::Make<UpdaterObserverImpl>();
  hr = updater->UpdateAll(observer.Get());
  if (FAILED(hr)) {
    VLOG(2) << "Failed to call IUpdater::UpdateAll" << std::hex << hr;
    std::move(callback).Run(static_cast<Result>(hr));
    return;
  }
}

}  // namespace updater
