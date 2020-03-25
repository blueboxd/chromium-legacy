// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}

namespace apps {

struct AppLaunchParams;

// A LaunchManager handles launch requests for a given type of apps.
class LaunchManager {
 public:
  virtual ~LaunchManager();

  // Open the application in a way specified by |params|.
  virtual content::WebContents* OpenApplication(
      const AppLaunchParams& params) = 0;

  // Attempt to open |app_id| in a new window or tab. Open an empty browser
  // window if unsuccessful. The user's preferred launch container for the app
  // (standalone window or browser tab) is used. |callback| will be called with
  // the container type used to open the app, kLaunchContainerNone if an empty
  // browser window was opened.
  virtual void LaunchApplication(
      const std::string& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory,
      base::OnceCallback<void(Browser* browser,
                              apps::mojom::LaunchContainer container)>
          callback) = 0;

 protected:
  explicit LaunchManager(Profile*);
  Profile* profile() { return profile_; }

 private:
  Profile* const profile_;
  DISALLOW_COPY_AND_ASSIGN(LaunchManager);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_
