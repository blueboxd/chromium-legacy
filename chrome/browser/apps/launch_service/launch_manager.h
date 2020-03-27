// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

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

 protected:
  explicit LaunchManager(Profile*);
  Profile* profile() { return profile_; }

 private:
  Profile* const profile_;
  DISALLOW_COPY_AND_ASSIGN(LaunchManager);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_MANAGER_H_
