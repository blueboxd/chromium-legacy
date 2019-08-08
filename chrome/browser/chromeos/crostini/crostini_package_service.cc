// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_package_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace crostini {

namespace {

class CrostiniPackageServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniPackageService* GetForProfile(Profile* profile) {
    return static_cast<CrostiniPackageService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniPackageServiceFactory* GetInstance() {
    static base::NoDestructor<CrostiniPackageServiceFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniPackageServiceFactory>;

  CrostiniPackageServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniPackageService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~CrostiniPackageServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniPackageService(profile);
  }
};

PackageOperationStatus InstallStatusToOperationStatus(
    InstallLinuxPackageProgressStatus status) {
  switch (status) {
    case InstallLinuxPackageProgressStatus::SUCCEEDED:
      return PackageOperationStatus::SUCCEEDED;
    case InstallLinuxPackageProgressStatus::FAILED:
      return PackageOperationStatus::FAILED;
    case InstallLinuxPackageProgressStatus::DOWNLOADING:
    case InstallLinuxPackageProgressStatus::INSTALLING:
      return PackageOperationStatus::RUNNING;
    default:
      NOTREACHED();
  }
}

PackageOperationStatus UninstallStatusToOperationStatus(
    UninstallPackageProgressStatus status) {
  switch (status) {
    case UninstallPackageProgressStatus::SUCCEEDED:
      return PackageOperationStatus::SUCCEEDED;
    case UninstallPackageProgressStatus::FAILED:
      return PackageOperationStatus::FAILED;
    case UninstallPackageProgressStatus::UNINSTALLING:
      return PackageOperationStatus::RUNNING;
    default:
      NOTREACHED();
  }
}

}  // namespace

struct CrostiniPackageService::QueuedUninstall {
  QueuedUninstall(
      const std::string& app_id,
      std::unique_ptr<CrostiniPackageNotification> notification_argument)
      : app_id(app_id), notification(std::move(notification_argument)) {}
  ~QueuedUninstall() = default;

  // App to uninstall
  std::string app_id;

  // Notification displaying "uninstall queued"
  std::unique_ptr<CrostiniPackageNotification> notification;
};

CrostiniPackageService* CrostiniPackageService::GetForProfile(
    Profile* profile) {
  return CrostiniPackageServiceFactory::GetForProfile(profile);
}

CrostiniPackageService::CrostiniPackageService(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile);

  manager->AddLinuxPackageOperationProgressObserver(this);
  manager->AddPendingAppListUpdatesObserver(this);
  manager->AddVmShutdownObserver(this);
}

CrostiniPackageService::~CrostiniPackageService() = default;

void CrostiniPackageService::Shutdown() {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->RemoveLinuxPackageOperationProgressObserver(this);
  manager->RemovePendingAppListUpdatesObserver(this);
  manager->RemoveVmShutdownObserver(this);

  // CrostiniPackageNotification registers itself as a CrostiniRegistryService
  // observer, so they need to be destroyed here while the
  // CrostiniRegistryService still exists.
  running_notifications_.clear();
  queued_uninstalls_.clear();
  finished_notifications_.clear();
}

void CrostiniPackageService::SetNotificationStateChangeCallbackForTesting(
    StateChangeCallback state_change_callback) {
  testing_state_change_callback_ = std::move(state_change_callback);
}

void CrostiniPackageService::NotificationCompleted(
    CrostiniPackageNotification* notification) {
  for (auto it = finished_notifications_.begin();
       it != finished_notifications_.end(); ++it) {
    if (it->get() == notification) {
      finished_notifications_.erase(it);
      return;
    }
  }
  // Notifications should never delete themselves while queued or running.
  NOTREACHED();
}

void CrostiniPackageService::GetLinuxPackageInfo(
    const std::string& vm_name,
    const std::string& container_name,
    const std::string& package_path,
    CrostiniManager::GetLinuxPackageInfoCallback callback) {
  CrostiniManager::GetForProfile(profile_)->GetLinuxPackageInfo(
      profile_, vm_name, container_name, package_path,
      base::BindOnce(&CrostiniPackageService::OnGetLinuxPackageInfo,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, container_name,
                     std::move(callback)));
}

void CrostiniPackageService::InstallLinuxPackage(
    const std::string& vm_name,
    const std::string& container_name,
    const std::string& package_path,
    CrostiniManager::InstallLinuxPackageCallback callback) {
  const ContainerId container_id(vm_name, container_name);
  containers_with_pending_installs_.insert(container_id);

  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackage(
      vm_name, container_name, package_path,
      base::BindOnce(&CrostiniPackageService::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, container_name,
                     std::move(callback)));
}

void CrostiniPackageService::OnInstallLinuxPackageProgress(
    const std::string& vm_name,
    const std::string& container_name,
    InstallLinuxPackageProgressStatus status,
    int progress_percent) {
  // Linux package install has two phases, downloading and installing, which we
  // map to a single progess percentage amount by dividing the range in half --
  // 0-50% for the downloading phase, 51-100% for the installing phase.
  int display_progress = progress_percent / 2;
  if (status == InstallLinuxPackageProgressStatus::INSTALLING)
    display_progress += 50;  // Second phase

  UpdatePackageOperationStatus(std::make_pair(vm_name, container_name),
                               InstallStatusToOperationStatus(status),
                               display_progress);
}

void CrostiniPackageService::OnUninstallPackageProgress(
    const std::string& vm_name,
    const std::string& container_name,
    UninstallPackageProgressStatus status,
    int progress_percent) {
  UpdatePackageOperationStatus(ContainerId(vm_name, container_name),
                               UninstallStatusToOperationStatus(status),
                               progress_percent);
}

void CrostiniPackageService::OnVmShutdown(const std::string& vm_name) {
  // Making a notification as failed removes it from |running_notifications_|,
  // which invalidates the iterators. To avoid this, we record all the
  // containers that just shut down before removing any notifications.
  std::vector<ContainerId> to_remove;
  for (auto iter = running_notifications_.begin();
       iter != running_notifications_.end(); iter++) {
    if (iter->first.first == vm_name) {
      to_remove.push_back(iter->first);
    }
  }
  for (auto iter : to_remove) {
    // Use a loop because removing a notification from the running set can cause
    // a queued operation to start, which will also need to be removed.
    while (ContainerHasRunningOperation(iter)) {
      UpdatePackageOperationStatus(iter, PackageOperationStatus::FAILED, 0);
    }
  }
}

void CrostiniPackageService::QueueUninstallApplication(
    const std::string& app_id) {
  auto registration =
      CrostiniRegistryServiceFactory::GetForProfile(profile_)->GetRegistration(
          app_id);
  DCHECK(registration);
  const std::string vm_name = registration->VmName();
  const std::string container_name = registration->ContainerName();
  const std::string app_name = registration->Name();

  const ContainerId container_id(vm_name, container_name);
  if (ContainerHasRunningOperation(container_id)) {
    CreateQueuedUninstall(container_id, app_id, app_name);
    return;
  }

  CreateRunningNotification(
      container_id,
      CrostiniPackageNotification::NotificationType::APPLICATION_UNINSTALL,
      app_name);

  UninstallApplication(*registration, app_id);
}

bool CrostiniPackageService::ContainerHasRunningOperation(
    const ContainerId& container_id) const {
  return base::Contains(running_notifications_, container_id) ||
         base::Contains(containers_with_pending_installs_, container_id);
}

void CrostiniPackageService::CreateRunningNotification(
    const ContainerId& container_id,
    CrostiniPackageNotification::NotificationType notification_type,
    const std::string& app_name) {
  {  // Scope limit for |it|, which will become invalid shortly.
    auto it = running_notifications_.find(container_id);
    if (it != running_notifications_.end()) {
      // We could reach this if the final progress update signal from a previous
      // operation doesn't get sent, so we wouldn't end up moving the
      // previous notification out of running_notifications_. Clear it out by
      // moving to finished_notifications_.
      LOG(ERROR) << "Notification for package operation already exists.";
      it->second->ForceAllowAutoHide();
      finished_notifications_.emplace_back(std::move(it->second));
      running_notifications_.erase(it);
    }
  }

  running_notifications_[container_id] =
      std::make_unique<CrostiniPackageNotification>(
          profile_, notification_type, PackageOperationStatus::RUNNING,
          container_id, base::UTF8ToUTF16(app_name), GetUniqueNotificationId(),
          this);
}

void CrostiniPackageService::CreateQueuedUninstall(
    const ContainerId& container_id,
    const std::string& app_id,
    const std::string& app_name) {
  queued_uninstalls_[container_id].emplace(
      app_id,
      std::make_unique<CrostiniPackageNotification>(
          profile_,
          CrostiniPackageNotification::NotificationType::APPLICATION_UNINSTALL,
          PackageOperationStatus::QUEUED, container_id,
          base::UTF8ToUTF16(app_name), GetUniqueNotificationId(), this));
}

void CrostiniPackageService::UpdatePackageOperationStatus(
    const ContainerId& container_id,
    PackageOperationStatus status,
    int progress_percent) {
  // Update the notification window, if any.
  auto it = running_notifications_.find(container_id);
  DCHECK(it != running_notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";
  DCHECK(it->second) << ContainerIdToString(container_id)
                     << " has null notification pointer";

  // If an operation has finished, but there are still app list updates pending,
  // don't finish the flow yet.
  if (status == PackageOperationStatus::SUCCEEDED &&
      has_pending_app_list_updates_.count(container_id)) {
    status = PackageOperationStatus::WAITING_FOR_APP_REGISTRY_UPDATE;
  }

  it->second->UpdateProgress(status, progress_percent);

  if (status == PackageOperationStatus::SUCCEEDED ||
      status == PackageOperationStatus::FAILED) {
    finished_notifications_.emplace_back(std::move(it->second));
    running_notifications_.erase(it);

    // Kick off the next operation if we just finished one.
    auto queued_iter = queued_uninstalls_.find(container_id);
    if (queued_iter != queued_uninstalls_.end() &&
        !queued_iter->second.empty()) {
      StartQueuedUninstall(container_id);
    }
  }
  if (testing_state_change_callback_) {
    testing_state_change_callback_.Run(status);
  }
}

void CrostiniPackageService::OnPendingAppListUpdates(
    const std::string& vm_name,
    const std::string& container_name,
    int count) {
  const ContainerId container_id(vm_name, container_name);

  if (count != 0) {
    has_pending_app_list_updates_.insert(container_id);
  } else {
    has_pending_app_list_updates_.erase(container_id);
  }

  auto it = running_notifications_.find(container_id);
  if (it != running_notifications_.end()) {
    if (it->second->GetOperationStatus() ==
            PackageOperationStatus::WAITING_FOR_APP_REGISTRY_UPDATE &&
        count == 0) {
      UpdatePackageOperationStatus(container_id,
                                   PackageOperationStatus::SUCCEEDED, 100);
    }
  }
}

void CrostiniPackageService::OnGetLinuxPackageInfo(
    const std::string& vm_name,
    const std::string& container_name,
    CrostiniManager::GetLinuxPackageInfoCallback callback,
    const LinuxPackageInfo& linux_package_info) {
  std::move(callback).Run(linux_package_info);
}

void CrostiniPackageService::OnInstallLinuxPackage(
    const std::string& vm_name,
    const std::string& container_name,
    CrostiniManager::InstallLinuxPackageCallback callback,
    CrostiniResult result) {
  std::move(callback).Run(result);
  const ContainerId container_id(vm_name, container_name);
  containers_with_pending_installs_.erase(container_id);
  if (result != CrostiniResult::SUCCESS) {
    // We never show a notification for this failed install, so this is our only
    // chance to kick off uninstalled queued behind the install.
    auto queued_iter = queued_uninstalls_.find(container_id);
    if (queued_iter != queued_uninstalls_.end() &&
        !queued_iter->second.empty()) {
      StartQueuedUninstall(container_id);
    }
    return;
  }
  CreateRunningNotification(
      container_id,
      CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
      "" /* app_name */);
}

void CrostiniPackageService::UninstallApplication(
    const CrostiniRegistryService::Registration& registration,
    const std::string& app_id) {
  const std::string vm_name = registration.VmName();
  const std::string container_name = registration.ContainerName();
  const ContainerId container_id(vm_name, container_name);

  // Policies can change under us, and crostini may now be forbidden.
  if (!IsCrostiniUIAllowedForProfile(profile_)) {
    LOG(ERROR) << "Can't uninstall because policy no longer allows Crostini";
    UpdatePackageOperationStatus(container_id, PackageOperationStatus::FAILED,
                                 0);
    return;
  }

  // If Crostini is not running, launch it. This is a no-op if Crostini is
  // already running.
  CrostiniManager::GetForProfile(profile_)->RestartCrostini(
      vm_name, container_name,
      base::BindOnce(&CrostiniPackageService::OnCrostiniRunningForUninstall,
                     weak_ptr_factory_.GetWeakPtr(), container_id,
                     registration.DesktopFileId()));
}

void CrostiniPackageService::OnCrostiniRunningForUninstall(
    const ContainerId& container_id,
    const std::string& desktop_file_id,
    CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to launch Crostini; uninstall aborted";
    UpdatePackageOperationStatus(container_id, PackageOperationStatus::FAILED,
                                 0);
    return;
  }
  const std::string& vm_name = container_id.first;
  const std::string& container_name = container_id.second;

  CrostiniManager::GetForProfile(profile_)->UninstallPackageOwningFile(
      vm_name, container_name, desktop_file_id,
      base::BindOnce(&CrostiniPackageService::OnUninstallPackageOwningFile,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void CrostiniPackageService::OnUninstallPackageOwningFile(
    const ContainerId& container_id,
    CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    // Let user know the uninstall failed.
    UpdatePackageOperationStatus(container_id, PackageOperationStatus::FAILED,
                                 0);
    return;
  }
  // Otherwise, just leave the notification alone in the "running" state.
  // SUCCESS just means we successfully *started* the uninstall.
}

void CrostiniPackageService::StartQueuedUninstall(
    const ContainerId& container_id) {
  std::string app_id;
  auto uninstall_queue_iter = queued_uninstalls_.find(container_id);
  if (uninstall_queue_iter == queued_uninstalls_.end()) {
    return;
  }
  std::queue<QueuedUninstall>& uninstall_queue = uninstall_queue_iter->second;
  {  // Scope |next|; it becomes an invalid reference when we pop()
    QueuedUninstall& next = uninstall_queue.front();

    next.notification->UpdateProgress(PackageOperationStatus::RUNNING, 0);
    running_notifications_.emplace(container_id, std::move(next.notification));

    app_id = next.app_id;
    uninstall_queue.pop();  // Invalidates |next|
  }

  auto registration =
      CrostiniRegistryServiceFactory::GetForProfile(profile_)->GetRegistration(
          app_id);

  // It's possible that some other process has uninstalled this application
  // already. If this happens, we want to skip the notification directly to the
  // success state.
  if (registration) {
    UninstallApplication(*registration, app_id);
  } else {
    UpdatePackageOperationStatus(container_id,
                                 PackageOperationStatus::SUCCEEDED, 100);
  }

  // Clean up memory.
  if (uninstall_queue.empty()) {
    queued_uninstalls_.erase(container_id);
    // Invalidates uninstall_queue
  }
}

std::string CrostiniPackageService::GetUniqueNotificationId() {
  return base::StringPrintf("crostini_package_operation_%d",
                            next_notification_id_++);
}

}  // namespace crostini
