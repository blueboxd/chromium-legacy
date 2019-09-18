// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handler_manager.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/components/web_app_file_extension_registration.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

FileHandlerManager::FileHandlerManager(Profile* profile)
    : profile_(profile), registrar_observer_(this) {}

FileHandlerManager::~FileHandlerManager() = default;

void FileHandlerManager::SetSubsystems(AppRegistrar* registrar) {
  registrar_ = registrar;
  registrar_observer_.Add(registrar);
}

void FileHandlerManager::OnWebAppInstalled(const AppId& installed_app_id) {
#if defined(OS_WIN)
  if (!base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI))
    return;
  std::string app_name = registrar_->GetAppShortName(installed_app_id);
  const std::vector<apps::FileHandlerInfo>* file_handlers =
      GetFileHandlers(installed_app_id);
  if (!file_handlers)
    return;
  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandlers(*file_handlers);
  RegisterFileHandlersForWebApp(installed_app_id, app_name, *profile_,
                                file_extensions);
#endif  // OS_WIN
}

void FileHandlerManager::OnWebAppUninstalled(const AppId& installed_app_id) {
#if defined(OS_WIN)
  UnregisterFileHandlersForWebApp(installed_app_id, *profile_);
#endif  // OS_WIN
}

void FileHandlerManager::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
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

}  // namespace web_app
