// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_file_handler_manager.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

WebAppFileHandlerManager::WebAppFileHandlerManager(Profile* profile)
    : FileHandlerManager(profile) {}

WebAppFileHandlerManager::~WebAppFileHandlerManager() = default;

const std::vector<apps::FileHandlerInfo>*
WebAppFileHandlerManager::GetAllFileHandlers(const AppId& app_id) {
  // Return cached FileHandlerInfo if present.
  auto it = file_handler_infos_.find(app_id);
  if (it != file_handler_infos_.end())
    return &it->second;

  WebAppRegistrar* web_app_registrar = registrar()->AsWebAppRegistrar();
  DCHECK(web_app_registrar);
  const WebApp* web_app = web_app_registrar->GetAppById(app_id);

  const std::vector<WebApp::FileHandler>& file_handlers =
      web_app->file_handlers();
  std::vector<apps::FileHandlerInfo> file_handler_infos;

  for (const auto& file_handler : file_handlers) {
    apps::FileHandlerInfo info;
    info.id = file_handler.action.spec();
    info.include_directories = false;
    info.verb = apps::file_handler_verbs::kOpenWith;

    for (const auto& accept_entry : file_handler.accept) {
      info.types.insert(accept_entry.mimetype);
      info.extensions.insert(accept_entry.file_extensions.begin(),
                             accept_entry.file_extensions.end());
    }

    file_handler_infos.push_back(std::move(info));
  }

  // The transformed data is stored in a map so that we can give a pointer to
  // the data to web_file_tasks, as they don't want the call to GetFileHandlers
  // to involve a copy.
  std::vector<apps::FileHandlerInfo>& apps_file_handler_infos =
      file_handler_infos_[app_id];
  apps_file_handler_infos = std::move(file_handler_infos);
  return &apps_file_handler_infos;
}

}  // namespace web_app
