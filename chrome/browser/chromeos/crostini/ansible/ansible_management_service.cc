// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace crostini {

namespace {

class AnsibleManagementServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AnsibleManagementService* GetForProfile(Profile* profile) {
    return static_cast<AnsibleManagementService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static AnsibleManagementServiceFactory* GetInstance() {
    static base::NoDestructor<AnsibleManagementServiceFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<AnsibleManagementServiceFactory>;

  AnsibleManagementServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "AnsibleManagementService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~AnsibleManagementServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new AnsibleManagementService(profile);
  }
};

chromeos::CiceroneClient* GetCiceroneClient() {
  return chromeos::DBusThreadManager::Get()->GetCiceroneClient();
}

}  // namespace

AnsibleManagementService* AnsibleManagementService::GetForProfile(
    Profile* profile) {
  return AnsibleManagementServiceFactory::GetForProfile(profile);
}

AnsibleManagementService::AnsibleManagementService(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}
AnsibleManagementService::~AnsibleManagementService() = default;

void AnsibleManagementService::InstallAnsibleInDefaultContainer(
    base::OnceCallback<void(bool success)> callback) {
  DCHECK(ansible_installation_finished_callback_.is_null());
  ansible_installation_finished_callback_ = std::move(callback);

  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackageFromApt(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      kCrostiniDefaultAnsibleVersion,
      base::BindOnce(
          &AnsibleManagementService::OnInstallAnsibleInDefaultContainer,
          weak_ptr_factory_.GetWeakPtr()));
}

void AnsibleManagementService::OnInstallAnsibleInDefaultContainer(
    CrostiniResult result) {
  if (result == CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED) {
    LOG(ERROR) << "Ansible installation failed";
    std::move(ansible_installation_finished_callback_).Run(/*success=*/false);
    return;
  }

  DCHECK_NE(result, CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);

  DCHECK_EQ(result, CrostiniResult::SUCCESS);
  VLOG(1) << "Ansible installation has been started successfully";
  // Waiting for Ansible installation progress being reported.
}

void AnsibleManagementService::OnInstallLinuxPackageProgress(
    const std::string& vm_name,
    const std::string& container_name,
    InstallLinuxPackageProgressStatus status,
    int progress_percent) {
  DCHECK_EQ(vm_name, kCrostiniDefaultVmName);
  DCHECK_EQ(container_name, kCrostiniDefaultContainerName);

  switch (status) {
    case InstallLinuxPackageProgressStatus::SUCCEEDED:
      std::move(ansible_installation_finished_callback_).Run(/*success=*/true);
      return;
    case InstallLinuxPackageProgressStatus::FAILED:
      std::move(ansible_installation_finished_callback_).Run(/*success=*/false);
      return;
    // TODO(okalitova): Report Ansible downloading/installation progress.
    case InstallLinuxPackageProgressStatus::DOWNLOADING:
      VLOG(1) << "Ansible downloading progress: " << progress_percent << "%";
      return;
    case InstallLinuxPackageProgressStatus::INSTALLING:
      VLOG(1) << "Ansible installing progress: " << progress_percent << "%";
      return;
    default:
      NOTREACHED();
  }
}

void AnsibleManagementService::OnUninstallPackageProgress(
    const std::string& vm_name,
    const std::string& container_name,
    UninstallPackageProgressStatus status,
    int progress_percent) {
  NOTIMPLEMENTED();
}

void AnsibleManagementService::ApplyAnsiblePlaybookToDefaultContainer(
    const std::string& playbook,
    base::OnceCallback<void(bool success)> callback) {
  if (!GetCiceroneClient()->IsApplyAnsiblePlaybookProgressSignalConnected()) {
    // Technically we could still start the application, but we wouldn't be able
    // to detect when the application completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to apply playbook when progress signal not connected.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  DCHECK(ansible_playbook_application_finished_callback_.is_null());
  ansible_playbook_application_finished_callback_ = std::move(callback);

  vm_tools::cicerone::ApplyAnsiblePlaybookRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_vm_name(std::move(kCrostiniDefaultContainerName));
  request.set_container_name(std::move(kCrostiniDefaultContainerName));
  request.set_playbook(std::move(playbook));

  GetCiceroneClient()->ApplyAnsiblePlaybook(
      std::move(request),
      base::BindOnce(&AnsibleManagementService::OnApplyAnsiblePlaybook,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AnsibleManagementService::OnApplyAnsiblePlaybook(
    base::Optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to apply Ansible playbook. Empty response.";
    std::move(ansible_playbook_application_finished_callback_)
        .Run(/*success=*/false);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED) {
    LOG(ERROR) << "Failed to apply Ansible playbook: "
               << response->failure_reason();
    std::move(ansible_playbook_application_finished_callback_)
        .Run(/*success=*/false);
    return;
  }

  VLOG(1) << "Ansible playbook application has been started successfully";
  // Waiting for Ansible playbook application progress being reported.
}

void AnsibleManagementService::OnApplyAnsiblePlaybookProgress(
    vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::Status status) {
  switch (status) {
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED:
      std::move(ansible_playbook_application_finished_callback_)
          .Run(/*success=*/true);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED:
      std::move(ansible_playbook_application_finished_callback_)
          .Run(/*success=*/false);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::IN_PROGRESS:
      // TODO(okalitova): Report Ansible playbook application progress.
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace crostini
