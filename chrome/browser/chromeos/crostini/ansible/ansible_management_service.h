// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

// TODO(okalitova): Install Ansible from backports repo once this is feasible.
constexpr char kCrostiniDefaultAnsibleVersion[] =
    "ansible;2.2.1.0-2+deb9u1;all;debian-stable-main";

// AnsibleManagementService is responsible for Crostini default
// container management using Ansible.
class AnsibleManagementService : public KeyedService,
                                 public LinuxPackageOperationProgressObserver {
 public:
  // Observer class for Ansible Management Service related events.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnAnsibleSoftwareConfigurationStarted() = 0;
    virtual void OnAnsibleSoftwareConfigurationFinished(bool success) = 0;
  };

  static AnsibleManagementService* GetForProfile(Profile* profile);

  explicit AnsibleManagementService(Profile* profile);
  ~AnsibleManagementService() override;

  // |callback| is called once Ansible installation is finished.
  void InstallAnsibleInDefaultContainer(
      base::OnceCallback<void(bool success)> callback);

  // |callback| is called once Ansible playbook application is finished.
  void ApplyAnsiblePlaybookToDefaultContainer(
      const std::string& playbook,
      base::OnceCallback<void(bool success)> callback);

  // LinuxPackageOperationProgressObserver:
  void OnInstallLinuxPackageProgress(const ContainerId& container_id,
                                     InstallLinuxPackageProgressStatus status,
                                     int progress_percent) override;
  void OnUninstallPackageProgress(const ContainerId& container_id,
                                  UninstallPackageProgressStatus status,
                                  int progress_percent) override;

  void OnApplyAnsiblePlaybookProgress(
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::Status status);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnInstallAnsibleInDefaultContainer(CrostiniResult result);

  // Callback for
  // CrostiniAnsibleManagementService::ApplyAnsiblePlaybookToDefaultContainer
  void OnApplyAnsiblePlaybook(
      base::Optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse>
          response);

  Profile* profile_;
  base::ObserverList<Observer> observers_;
  base::OnceCallback<void(bool success)>
      ansible_installation_finished_callback_;
  base::OnceCallback<void(bool success)>
      ansible_playbook_application_finished_callback_;
  base::WeakPtrFactory<AnsibleManagementService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AnsibleManagementService);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
