// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_EXTENSION_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_EXTENSION_REGISTRATION_H_

#include <set>
#include <string>

#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Profile;

namespace web_app {

// Do OS-specific registration to handle opening files with the specified
// |file_extensions| with the PWA with the specified |app_id|.
// This may also involve creating a shim app to launch Chrome from.
void RegisterFileHandlersForWebApp(
    const AppId& app_id,
    const std::string& app_name,
    const Profile& profile,
    const std::set<std::string>& file_extensions);

// Undo the file extensions registration for the PWA with specified |app_id|.
// If a shim app was required, also removes the shim app.
void UnregisterFileHandlersForWebApp(const AppId& app_id,
                                     const Profile& profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_EXTENSION_REGISTRATION_H_
