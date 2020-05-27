// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeDlcserviceClient::FakeDlcserviceClient() = default;

FakeDlcserviceClient::~FakeDlcserviceClient() = default;

void FakeDlcserviceClient::Install(const std::string& dlc_id,
                                   InstallCallback callback,
                                   ProgressCallback progress_callback) {
  VLOG(1) << "Requesting to install DLC(s).";
  InstallResult install_result{
      .error = install_err_,
      .dlc_id = dlc_id,
      .root_path = "",
  };
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(install_result)));
}

void FakeDlcserviceClient::Uninstall(const std::string& dlc_id,
                                     UninstallCallback callback) {
  VLOG(1) << "Requesting to uninstall DLC=" << dlc_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), uninstall_err_));
}

void FakeDlcserviceClient::Purge(const std::string& dlc_id,
                                 PurgeCallback callback) {
  VLOG(1) << "Requesting to purge DLC=" << dlc_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), purge_err_));
}

void FakeDlcserviceClient::GetExistingDlcs(GetExistingDlcsCallback callback) {
  VLOG(1) << "Requesting to get existing DLC(s).";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), get_existing_dlcs_err_,
                                dlcs_with_content_));
}

void FakeDlcserviceClient::DlcStateChangedForTest(dbus::Signal* signal) {
  NOTREACHED();
}

}  // namespace chromeos
