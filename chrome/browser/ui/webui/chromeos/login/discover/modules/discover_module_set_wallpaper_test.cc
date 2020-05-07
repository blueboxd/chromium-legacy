// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace chromeos {

class DiscoverModuleSetWallpaperTest : public DiscoverBrowserTest {
 protected:
  DiscoverModuleSetWallpaperTest() {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

  ~DiscoverModuleSetWallpaperTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiscoverModuleSetWallpaperTest);
};

IN_PROC_BROWSER_TEST_F(DiscoverModuleSetWallpaperTest, LaunchWallpaperApp) {
  LoadAndInitializeDiscoverApp(ProfileManager::GetPrimaryUserProfile());

  ExtensionTestMessageListener window_created_listener(
      "wallpaper-window-created", false);
  ExtensionTestMessageListener launched_listener("launched", false);

  ClickOnCard("discover-set-wallpaper-card");
  EXPECT_TRUE(window_created_listener.WaitUntilSatisfied())
      << "Wallpaper picker window was not created.";
  EXPECT_TRUE(launched_listener.WaitUntilSatisfied())
      << "Wallpaper picker was not loaded.";
}

}  // namespace chromeos
