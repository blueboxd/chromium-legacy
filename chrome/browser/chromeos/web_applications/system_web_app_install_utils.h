// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_SYSTEM_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_SYSTEM_WEB_APP_INSTALL_UTILS_H_

#include "chrome/common/web_application_info.h"
#include "url/gurl.h"

enum class WebappInstallSource;
struct WebApplicationInfo;

namespace web_app {
// Create the icon info struct for a system web app from a resource id. We don't
// actually download the icon, so the app_url and icon_name are just used as a
// key.
void CreateIconInfoForSystemWebApp(const GURL& app_url,
                                   const std::string& icon_name,
                                   SquareSizePx square_size_px,
                                   int resource_id,
                                   WebApplicationInfo& web_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_SYSTEM_WEB_APP_INSTALL_UTILS_H_
