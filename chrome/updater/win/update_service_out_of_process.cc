// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/update_service_out_of_process.h"

#include <windows.h>
#include <wrl/client.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/server/win/updater_idl.h"

namespace {

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

namespace updater {

HRESULT UpdaterObserverImpl::OnComplete(int error_code) {
  VLOG(2) << "UpdaterObserverImpl::OnComplete(" << error_code << ")";
  return S_OK;
}

UpdateServiceOutOfProcess::UpdateServiceOutOfProcess() = default;

UpdateServiceOutOfProcess::~UpdateServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<UpdateServiceOutOfProcess>
UpdateServiceOutOfProcess::CreateInstance() {
  struct Creator : public UpdateServiceOutOfProcess {};
  auto instance = std::make_unique<Creator>();
  instance->com_task_runner_ =
      base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits);
  if (!instance->com_task_runner_)
    return nullptr;
  return instance;
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

  ::CoAddRefServerProcess();
  auto observer = Microsoft::WRL::Make<UpdaterObserverImpl>();
  hr = updater->UpdateAll(observer.Get());
  if (FAILED(hr)) {
    VLOG(2) << "Failed to call IUpdater::UpdateAll" << std::hex << hr;
    std::move(callback).Run(static_cast<Result>(hr));
    return;
  }

  observer.Reset();
  ::CoReleaseServerProcess();
}

}  // namespace updater
