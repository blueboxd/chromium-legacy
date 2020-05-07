// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace plugin_vm {

namespace {

void DirExistsResult(
    const base::FilePath& dir,
    bool result,
    base::OnceCallback<void(const base::FilePath&, bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(dir, result);
}

void EnsureDirExists(
    base::FilePath dir,
    base::OnceCallback<void(const base::FilePath&, bool)> callback) {
  base::File::Error error = base::File::FILE_OK;
  bool result = base::CreateDirectoryAndGetError(dir, &error);
  if (!result) {
    LOG(ERROR) << "Failed to create PluginVm shared dir " << dir.value() << ": "
               << base::File::ErrorToString(error);
  }
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&DirExistsResult, dir, result, std::move(callback)));
}

base::FilePath GetDefaultSharedDir(Profile* profile) {
  return file_manager::util::GetMyFilesFolderForProfile(profile).Append(
      kPluginVmName);
}

// LaunchPluginVmApp will run before this and try to start Plugin VM.
void LaunchPluginVmAppImpl(Profile* profile,
                           std::string app_id,
                           const std::vector<storage::FileSystemURL>& files,
                           LaunchPluginVmAppCallback callback,
                           bool plugin_vm_is_running) {
  if (!plugin_vm_is_running) {
    return std::move(callback).Run(false, "Plugin VM could not be started");
  }

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  auto registration = registry_service->GetRegistration(app_id);
  if (!registration) {
    return std::move(callback).Run(
        false, "LaunchPluginVmApp called with an unknown app_id: " + app_id);
  }

  std::vector<std::string> file_paths;
  file_paths.reserve(files.size());
  for (const auto& file : files) {
    auto file_path =
        ConvertFileSystemURLToPathInsidePluginVmSharedDir(profile, file);
    if (!file_path) {
      return std::move(callback).Run(
          false, "Only files in the shared dir are supported. Got: " +
                     file.DebugString());
    }
    file_paths.push_back(std::move(*file_path));
  }

  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile));
  request.set_vm_name(registration->VmName());
  request.set_container_name(registration->ContainerName());
  request.set_desktop_file_id(registration->DesktopFileId());
  std::copy(
      std::make_move_iterator(file_paths.begin()),
      std::make_move_iterator(file_paths.end()),
      google::protobuf::RepeatedFieldBackInserter(request.mutable_files()));

  chromeos::DBusThreadManager::Get()
      ->GetCiceroneClient()
      ->LaunchContainerApplication(
          std::move(request),
          base::BindOnce(
              [](const std::string& app_id, LaunchPluginVmAppCallback callback,
                 base::Optional<
                     vm_tools::cicerone::LaunchContainerApplicationResponse>
                     response) {
                if (!response || !response->success()) {
                  LOG(ERROR) << "Failed to launch application. "
                             << (response ? response->failure_reason()
                                          : "Empty response.");
                  std::move(callback).Run(/*success=*/false,
                                          "Failed to launch " + app_id);
                  return;
                }

                std::move(callback).Run(/*success=*/true, "");
              },
              std::move(app_id), std::move(callback)));
}

}  // namespace

void EnsureDefaultSharedDirExists(
    Profile* profile,
    base::OnceCallback<void(const base::FilePath&, bool)> callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&EnsureDirExists, GetDefaultSharedDir(profile),
                     std::move(callback)));
}

base::Optional<std::string> ConvertFileSystemURLToPathInsidePluginVmSharedDir(
    Profile* profile,
    const storage::FileSystemURL& file_system_url) {
  auto path = file_system_url.path();
  if (!GetDefaultSharedDir(profile).IsParent(path)) {
    return base::nullopt;
  }

  using Components = std::vector<base::FilePath::StringType>;

  Components components;
  path.GetComponents(&components);

  // TODO(juwa): reuse MyFiles constant from file_manager/path_util.cc.
  Components::iterator vm_components_start =
      std::find(components.begin(), components.end(), "MyFiles");

  Components vm_components;
  vm_components.reserve(3 +
                        std::distance(vm_components_start, components.end()));
  vm_components.emplace_back();
  vm_components.emplace_back();
  vm_components.emplace_back("ChromeOS");
  std::copy(std::make_move_iterator(vm_components_start),
            std::make_move_iterator(components.end()),
            std::back_inserter(vm_components));
  return base::JoinString(std::move(vm_components), "\\");
}

void LaunchPluginVmApp(Profile* profile,
                       std::string app_id,
                       std::vector<storage::FileSystemURL> files,
                       LaunchPluginVmAppCallback callback) {
  if (!plugin_vm::IsPluginVmEnabled(profile)) {
    return std::move(callback).Run(false,
                                   "Plugin VM is not enabled for this profile");
  }

  auto* manager = PluginVmManagerFactory::GetForProfile(profile);

  if (!manager) {
    return std::move(callback).Run(false, "Could not get PluginVmManager");
  }

  manager->LaunchPluginVm(base::BindOnce(&LaunchPluginVmAppImpl, profile,
                                         std::move(app_id), std::move(files),
                                         std::move(callback)));
}

}  // namespace plugin_vm
