// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handler_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

FileHandlerManager::FileHandlerManager(Profile* profile)
    : profile_(profile), registrar_observer_(this) {}

FileHandlerManager::~FileHandlerManager() = default;

void FileHandlerManager::SetSubsystems(AppRegistrar* registrar) {
  registrar_ = registrar;
}

void FileHandlerManager::Start() {
  DCHECK(registrar_);

  registrar_observer_.Add(registrar_);
}

void FileHandlerManager::DisableOsIntegrationForTesting() {
  disable_os_integration_for_testing_ = true;
}

void FileHandlerManager::EnableAndRegisterOsFileHandlers(const AppId& app_id) {
  if (!IsFileHandlingAPIAvailable(app_id))
    return;

  UpdateBoolWebAppPref(profile()->GetPrefs(), app_id, kFileHandlersEnabled,
                       /*value=*/true);

  if (!ShouldRegisterFileHandlersWithOs() ||
      disable_os_integration_for_testing_) {
    return;
  }

  std::string app_name = registrar_->GetAppShortName(app_id);
  const std::vector<apps::FileHandlerInfo>* file_handlers =
      GetAllFileHandlers(app_id);
  if (!file_handlers)
    return;
  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandlers(*file_handlers);
  std::set<std::string> mime_types =
      GetMimeTypesFromFileHandlers(*file_handlers);
  RegisterFileHandlersWithOs(app_id, app_name, profile(), file_extensions,
                             mime_types);
}

void FileHandlerManager::DisableAndUnregisterOsFileHandlers(
    const AppId& app_id) {
  UpdateBoolWebAppPref(profile()->GetPrefs(), app_id, kFileHandlersEnabled,
                       /*value=*/false);

  if (!ShouldRegisterFileHandlersWithOs() ||
      disable_os_integration_for_testing_) {
    return;
  }

  UnregisterFileHandlersWithOs(app_id, profile());
}

const std::vector<apps::FileHandlerInfo>*
FileHandlerManager::GetEnabledFileHandlers(const AppId& app_id) {
  if (AreFileHandlersEnabled(app_id) && IsFileHandlingAPIAvailable(app_id))
    return GetAllFileHandlers(app_id);

  return nullptr;
}

bool FileHandlerManager::IsFileHandlingAPIAvailable(const AppId& app_id) {
  return base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI);
}

bool FileHandlerManager::AreFileHandlersEnabled(const AppId& app_id) const {
  return GetBoolWebAppPref(profile()->GetPrefs(), app_id, kFileHandlersEnabled);
}

void FileHandlerManager::OnWebAppUninstalled(const AppId& app_id) {
  DisableAndUnregisterOsFileHandlers(app_id);
}

void FileHandlerManager::OnWebAppProfileWillBeDeleted(const AppId& app_id) {
  DisableAndUnregisterOsFileHandlers(app_id);
}

void FileHandlerManager::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

const base::Optional<GURL> FileHandlerManager::GetMatchingFileHandlerURL(
    const AppId& app_id,
    const std::vector<base::FilePath>& launch_files) {
  if (!IsFileHandlingAPIAvailable(app_id))
    return base::nullopt;

  const std::vector<apps::FileHandlerInfo>* file_handlers =
      GetAllFileHandlers(app_id);
  if (!file_handlers || launch_files.empty())
    return base::nullopt;

  // Leading `.` for each file extension must be removed to match those given by
  // FileHandlerInfo.extensions below.
  std::set<std::string> file_extensions;
  for (const auto& file_path : launch_files) {
    std::string extension =
        base::FilePath(file_path.Extension()).AsUTF8Unsafe();
    if (extension.length() <= 1)
      return base::nullopt;
    file_extensions.insert(extension.substr(1));
  }

  for (const auto& file_handler : *file_handlers) {
    bool all_extensions_supported = true;
    for (const auto& extension : file_extensions) {
      if (!file_handler.extensions.count(extension)) {
        all_extensions_supported = false;
        break;
      }
    }
    if (all_extensions_supported)
      return GURL(file_handler.id);
  }

  return base::nullopt;
}

std::set<std::string> GetFileExtensionsFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers) {
  std::set<std::string> file_extensions;
  for (const auto& file_handler : file_handlers) {
    for (const auto& file_ext : file_handler.extensions)
      file_extensions.insert(file_ext);
  }
  return file_extensions;
}

std::set<std::string> GetMimeTypesFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers) {
  std::set<std::string> mime_types;
  for (const auto& file_handler : file_handlers) {
    for (const auto& mime_type : file_handler.types)
      mime_types.insert(mime_type);
  }
  return mime_types;
}

}  // namespace web_app
