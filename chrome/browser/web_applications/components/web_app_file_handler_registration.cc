// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace web_app {

#if !defined(OS_WIN)
bool OsSupportsWebAppFileHandling() {
  return false;
}

void RegisterFileHandlersForWebApp(const AppId& app_id,
                                   const std::string& app_name,
                                   const Profile& profile,
                                   const std::set<std::string>& file_extensions,
                                   const std::set<std::string>& mime_types) {
  DCHECK(OsSupportsWebAppFileHandling());
  // Stub function for OS's that don't support Web App file handling yet.
}

void UnregisterFileHandlersForWebApp(const AppId& app_id,
                                     const Profile& profile) {
  DCHECK(OsSupportsWebAppFileHandling());
  // Stub function for OS's that don't support Web App file handling yet.
}
#endif

}  // namespace web_app
