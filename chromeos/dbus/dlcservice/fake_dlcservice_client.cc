// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeDlcserviceClient::FakeDlcserviceClient() = default;

FakeDlcserviceClient::~FakeDlcserviceClient() = default;

void FakeDlcserviceClient::Install(
    const dlcservice::DlcModuleList& dlc_module_list,
    InstallCallback callback,
    ProgressCallback progress_callback) {
  VLOG(1) << "Requesting to install DLC(s).";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), dlcservice::kErrorNone,
                                dlcservice::DlcModuleList()));
}

void FakeDlcserviceClient::Uninstall(const std::string& dlc_id,
                                     UninstallCallback callback) {
  VLOG(1) << "Requesting to uninstall DLC=" << dlc_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), dlcservice::kErrorNone));
}

void FakeDlcserviceClient::GetInstalled(GetInstalledCallback callback) {
  VLOG(1) << "Requesting to get installed DLC(s).";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), dlcservice::kErrorNone,
                                dlcservice::DlcModuleList()));
}

void FakeDlcserviceClient::OnInstallStatusForTest(dbus::Signal* signal) {
  NOTREACHED();
}

}  // namespace chromeos
