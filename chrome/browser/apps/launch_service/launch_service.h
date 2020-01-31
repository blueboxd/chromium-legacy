// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
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

class LaunchManager;
struct AppLaunchParams;

// This KeyedService receives app launch requests and forwards them
// to the appropriate LaunchManager, based on the type of app.
//
// It is expected to merge into the App Service (Proxy) when that service
// stabilizes. Launch requests will be forwarded through App publishers to App
// providers, and the LaunchManager classes will be retired. See
// chrome/services/app_service/README.md
class LaunchService : public KeyedService {
 public:
  static LaunchService* Get(Profile* profile);

  explicit LaunchService(Profile* profile);
  ~LaunchService() override;

  // Open the application in a way specified by |params|.
  content::WebContents* OpenApplication(const AppLaunchParams& params);

  // Attempt to open |app_id| in a new window or tab. Open an empty browser
  // window if unsuccessful. The user's preferred launch container for the app
  // (standalone window or browser tab) is used. |callback| will be called with
  // the container type used to open the app, kLaunchContainerNone if an empty
  // browser window was opened.
  void LaunchApplication(
      const std::string& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory,
      base::OnceCallback<void(Browser* browser,
                              apps::mojom::LaunchContainer container)>
          callback);

 private:
  LaunchManager& GetLaunchManagerForApp(const std::string& app_id);

  Profile* const profile_;
  std::unique_ptr<LaunchManager> extension_app_launch_manager_;
  std::unique_ptr<LaunchManager> web_app_launch_manager_;

  DISALLOW_COPY_AND_ASSIGN(LaunchService);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_H_
