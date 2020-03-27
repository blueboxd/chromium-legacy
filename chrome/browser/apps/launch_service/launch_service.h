// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

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

 private:
  LaunchManager& GetLaunchManagerForApp(const std::string& app_id);

  Profile* const profile_;
  std::unique_ptr<LaunchManager> extension_app_launch_manager_;
  std::unique_ptr<LaunchManager> web_app_launch_manager_;

  DISALLOW_COPY_AND_ASSIGN(LaunchService);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_H_
