// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_ansible_management_service.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace crostini {

namespace {

class CrostiniAnsibleManagementServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniAnsibleManagementService* GetForProfile(Profile* profile) {
    return static_cast<CrostiniAnsibleManagementService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniAnsibleManagementServiceFactory* GetInstance() {
    static base::NoDestructor<CrostiniAnsibleManagementServiceFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniAnsibleManagementServiceFactory>;

  CrostiniAnsibleManagementServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniAnsibleManagementService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~CrostiniAnsibleManagementServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniAnsibleManagementService(profile);
  }
};

}  // namespace

CrostiniAnsibleManagementService*
CrostiniAnsibleManagementService::GetForProfile(Profile* profile) {
  return CrostiniAnsibleManagementServiceFactory::GetForProfile(profile);
}

CrostiniAnsibleManagementService::CrostiniAnsibleManagementService(
    Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}
CrostiniAnsibleManagementService::~CrostiniAnsibleManagementService() = default;

void CrostiniAnsibleManagementService::InstallAnsibleInDefaultContainer(
    base::OnceCallback<void(bool success)> callback) {
  DCHECK(ansible_installation_finished_callback_.is_null());
  ansible_installation_finished_callback_ = std::move(callback);

  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackageFromApt(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      kCrostiniDefaultAnsibleVersion,
      base::BindOnce(
          &CrostiniAnsibleManagementService::OnInstallAnsibleInDefaultContainer,
          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniAnsibleManagementService::OnInstallAnsibleInDefaultContainer(
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

void CrostiniAnsibleManagementService::OnInstallLinuxPackageProgress(
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

void CrostiniAnsibleManagementService::OnUninstallPackageProgress(
    const std::string& vm_name,
    const std::string& container_name,
    UninstallPackageProgressStatus status,
    int progress_percent) {
  NOTIMPLEMENTED();
}

void CrostiniAnsibleManagementService::ApplyAnsiblePlaybookToDefaultContainer(
    const std::string& playbook) {
  NOTIMPLEMENTED();
}

}  // namespace crostini
