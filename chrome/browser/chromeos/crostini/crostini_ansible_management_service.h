// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_ANSIBLE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_ANSIBLE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

// TODO(okalitova): Install Ansible from backports repo once this is feasible.
constexpr char kCrostiniDefaultAnsibleVersion[] =
    "ansible;2.2.1.0-2+deb9u1;all;debian-stable-main";

// CrostiniAnsibleManagementService is responsible for Crostini default
// container management using Ansible.
class CrostiniAnsibleManagementService
    : public KeyedService,
      public LinuxPackageOperationProgressObserver {
 public:
  static CrostiniAnsibleManagementService* GetForProfile(Profile* profile);

  explicit CrostiniAnsibleManagementService(Profile* profile);
  ~CrostiniAnsibleManagementService() override;

  void InstallAnsibleInDefaultContainer(
      base::OnceCallback<void(bool success)> callback);

  void ApplyAnsiblePlaybookToDefaultContainer(const std::string& playbook);

  // LinuxPackageOperationProgressObserver:
  void OnInstallLinuxPackageProgress(const std::string& vm_name,
                                     const std::string& container_name,
                                     InstallLinuxPackageProgressStatus status,
                                     int progress_percent) override;
  void OnUninstallPackageProgress(const std::string& vm_name,
                                  const std::string& container_name,
                                  UninstallPackageProgressStatus status,
                                  int progress_percent) override;

 private:
  void OnInstallAnsibleInDefaultContainer(CrostiniResult result);

  Profile* profile_;
  base::OnceCallback<void(bool success)>
      ansible_installation_finished_callback_;
  base::WeakPtrFactory<CrostiniAnsibleManagementService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAnsibleManagementService);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_ANSIBLE_MANAGEMENT_SERVICE_H_
