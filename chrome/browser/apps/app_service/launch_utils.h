// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_

#include <string>
#include <vector>

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace apps {

std::string GetAppIdForWebContents(content::WebContents* web_contents);

bool IsInstalledApp(Profile* profile, const std::string& app_id);

void SetAppIdForWebContents(Profile* profile,
                            content::WebContents* web_contents,
                            const std::string& app_id);

// Converts file arguments to an app on |command_line| into base::FilePaths.
std::vector<base::FilePath> GetLaunchFilesFromCommandLine(
    const base::CommandLine& command_line);

// When a command line launch has an unknown app id, we open a browser with only
// the new tab page.
Browser* CreateBrowserWithNewTabPage(Profile* profile);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_
