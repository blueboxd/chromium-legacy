// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handler_manager.h"

namespace web_app {

FileHandlerManager::FileHandlerManager(Profile* profile)
    : profile_(profile), registrar_observer_(this) {}

FileHandlerManager::~FileHandlerManager() = default;

void FileHandlerManager::SetSubsystems(AppRegistrar* registrar) {
  registrar_ = registrar;
  registrar_observer_.Add(registrar);
}

void FileHandlerManager::OnWebAppInstalled(const AppId& installed_app_id) {
  // TODO(davidbienvenu): Hook up with file association registration code on
  // windows.
}

void FileHandlerManager::OnWebAppUninstalled(const AppId& installed_app_id) {
  // TODO(davidbienvenu): Hook up to file association de-registration code on
  // windows.
}

void FileHandlerManager::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

}  // namespace web_app
