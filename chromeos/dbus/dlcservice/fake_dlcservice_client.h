// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_
#define CHROMEOS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace chromeos {

// A fake implementation of DlcserviceClient.
class COMPONENT_EXPORT(DLCSERVICE_CLIENT) FakeDlcserviceClient
    : public DlcserviceClient {
 public:
  FakeDlcserviceClient();
  ~FakeDlcserviceClient() override;

  // DlcserviceClient:
  void Install(const dlcservice::DlcModuleList& dlc_module_list,
               InstallCallback callback,
               ProgressCallback progress_callback) override;
  // Uninstalling disables the DLC.
  void Uninstall(const std::string& dlc_id,
                 UninstallCallback callback) override;
  // Purging removes the DLC entirely from disk.
  void Purge(const std::string& dlc_id, PurgeCallback callback) override;
  void GetInstalled(GetInstalledCallback callback) override;
  void GetExistingDlcs(GetExistingDlcsCallback callback) override;
  void OnInstallStatusForTest(dbus::Signal* signal) override;

  // Setters:
  // TODO(hsuregan/kimjae): Convert setters and at tests that use them to
  // underscore style instead of camel case.
  void SetInstallError(const std::string& err) { install_err_ = err; }
  void SetUninstallError(const std::string& err) { uninstall_err_ = err; }
  void SetPurgeError(const std::string& err) { purge_err_ = err; }
  void SetGetInstalledError(const std::string& err) {
    get_installed_err_ = err;
  }
  void SetGetExistingDlcsError(const std::string& err) {
    get_existing_dlcs_err_ = err;
  }
  void set_installed_dlcs(const dlcservice::DlcModuleList& dlc_module_list) {
    dlc_module_list_ = dlc_module_list;
  }
  void set_dlcs_with_content(
      const dlcservice::DlcsWithContent& dlcs_with_content) {
    dlcs_with_content_ = dlcs_with_content;
  }

 private:
  std::string install_err_ = dlcservice::kErrorNone;
  std::string uninstall_err_ = dlcservice::kErrorNone;
  std::string purge_err_ = dlcservice::kErrorNone;
  std::string get_installed_err_ = dlcservice::kErrorNone;
  dlcservice::DlcModuleList dlc_module_list_;
  std::string get_existing_dlcs_err_ = dlcservice::kErrorNone;
  dlcservice::DlcsWithContent dlcs_with_content_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_
