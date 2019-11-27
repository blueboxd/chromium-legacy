// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

namespace web_app {

bool OsSupportsWebAppFileHandling() {
  return true;
}

void RegisterFileHandlersForWebApp(const AppId& app_id,
                                   const std::string& app_name,
                                   Profile* profile,
                                   const std::set<std::string>& file_extensions,
                                   const std::set<std::string>& mime_types) {
  // ChromeOS talks directly to FileHandlerManager for file handler
  // registrations.
}

void UnregisterFileHandlersForWebApp(const AppId& app_id, Profile* profile) {
  // ChromeOS talks directly to FileHandlerManager for file handler
  // registrations.
}

}  // namespace web_app
