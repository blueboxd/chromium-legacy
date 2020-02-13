// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_APP_UTILS_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_APP_UTILS_H_

#include <string>

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace apps {

std::string GetAppIdForWebContents(content::WebContents* web_contents);

bool IsInstalledApp(Profile* profile, const std::string& app_id);

void SetAppIdForWebContents(Profile* profile,
                            content::WebContents* web_contents,
                            const std::string& app_id);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_APP_UTILS_H_
