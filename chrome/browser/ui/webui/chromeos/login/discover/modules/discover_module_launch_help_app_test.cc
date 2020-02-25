// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_browser_test.h"

namespace chromeos {

using DiscoverModuleLaunchHelpAppTest = DiscoverBrowserTest;

IN_PROC_BROWSER_TEST_F(DiscoverModuleLaunchHelpAppTest, ShowHelpTab) {
  LoadAndInitializeDiscoverApp(ProfileManager::GetPrimaryUserProfile());

  base::RunLoop run_loop;

  base::ActionCallback on_user_action = base::BindLambdaForTesting(
      [&](const std::string& action, base::TimeTicks action_time) {
        if (action == "ShowHelpTab")
          run_loop.Quit();
      });
  base::AddActionCallback(on_user_action);

  ClickOnCard("discover-launch-help-app-card");
  run_loop.Run();

  base::RemoveActionCallback(on_user_action);
}

}  // namespace chromeos
