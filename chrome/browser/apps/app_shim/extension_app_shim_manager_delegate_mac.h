// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_MANAGER_DELEGATE_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_MANAGER_DELEGATE_MAC_H_

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"

namespace apps {

class ExtensionAppShimManagerDelegate : public AppShimManager::Delegate {
 public:
  ExtensionAppShimManagerDelegate();
  ~ExtensionAppShimManagerDelegate() override;

  // Return the profile for |path|, only if it is already loaded.
  Profile* ProfileForPath(const base::FilePath& path) override;

  // Load a profile and call |callback| when completed or failed.
  void LoadProfileAsync(const base::FilePath& path,
                        base::OnceCallback<void(Profile*)> callback) override;

  // Return true if the specified path is for a valid profile that is also
  // locked.
  bool IsProfileLockedForPath(const base::FilePath& path) override;

  // Show all app windows (for non-PWA apps). Return true if there existed any
  // windows.
  bool ShowAppWindows(Profile* profile, const web_app::AppId& app_id) override;

  // Close all app windows (for non-PWA apps).
  void CloseAppWindows(Profile* profile, const web_app::AppId& app_id) override;

  // Return true iff |app_id| corresponds to an app that is installed for
  // |profile|.
  bool AppIsInstalled(Profile* profile, const web_app::AppId& app_id) override;

  // Return true iff the specified app can create an AppShimHost, which will
  // keep the app shim process connected (as opposed to, e.g, a bookmark app
  // that opens in a tab, which will immediately close).
  bool AppCanCreateHost(Profile* profile,
                        const web_app::AppId& app_id) override;

  // Return true if Cocoa windows for this app should be hosted in the app
  // shim process.
  bool AppUsesRemoteCocoa(Profile* profile,
                          const web_app::AppId& app_id) override;

  // Return true if a single app shim is used for all profiles (as opposed to
  // one shim per profile).
  bool AppIsMultiProfile(Profile* profile,
                         const web_app::AppId& app_id) override;

  // Create an AppShimHost for the specified parameters (intercept-able for
  // tests).
  std::unique_ptr<AppShimHost> CreateHost(AppShimHost::Client* client,
                                          const base::FilePath& profile_path,
                                          const web_app::AppId& app_id,
                                          bool use_remote_cocoa) override;

  // Open a dialog to enable the specified extension. Call |callback| after
  // the dialog is executed.
  void EnableExtension(Profile* profile,
                       const std::string& extension_id,
                       base::OnceCallback<void()> callback) override;

  // Launch the app in Chrome. This will (often) create a new window.
  void LaunchApp(Profile* profile,
                 const web_app::AppId& app_id,
                 const std::vector<base::FilePath>& files) override;

  // Open the specified URL in a new Chrome window. This is the fallback when
  // an app shim exists, but there is no profile or extension for it. If
  // |profile_path| is specified, then that profile is preferred, otherwise,
  // the last used profile is used.
  void OpenAppURLInBrowserWindow(const base::FilePath& profile_path,
                                 const GURL& url) override;

  // Launch the shim process for an app.
  void LaunchShim(Profile* profile,
                  const web_app::AppId& app_id,
                  bool recreate_shims,
                  ShimLaunchedCallback launched_callback,
                  ShimTerminatedCallback terminated_callback) override;

  // Launch the user manager (in response to attempting to access a locked
  // profile).
  void LaunchUserManager() override;

  // Terminate Chrome if Chrome attempted to quit, but was prevented from
  // quitting due to apps being open.
  void MaybeTerminate() override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_MANAGER_DELEGATE_MAC_H_
