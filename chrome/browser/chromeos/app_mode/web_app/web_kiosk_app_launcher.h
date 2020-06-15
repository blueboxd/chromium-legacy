// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class Browser;
class BrowserWindow;
class Profile;

namespace web_app {
class WebAppInstallTask;
class WebAppUrlLoader;
class WebAppDataRetriever;
}  // namespace web_app

namespace chromeos {

class WebKioskAppData;

// Object responsible for preparing and launching web kiosk app. Is destroyed
// upon app launch.
class WebKioskAppLauncher {
 public:
  class Delegate {
   public:
    virtual void InitializeNetwork() = 0;
    virtual void OnAppStartedInstalling() = 0;
    virtual void OnAppPrepared() = 0;
    virtual void OnAppLaunched() = 0;
    virtual void OnAppLaunchFailed(KioskAppLaunchError::Error error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  WebKioskAppLauncher(Profile* profile,
                      Delegate* delegate,
                      const AccountId& account_id);
  virtual ~WebKioskAppLauncher();

  // Prepares the environment for an app launch.
  virtual void Initialize();
  // Continues the installation when the network is ready.
  virtual void ContinueWithNetworkReady();
  // Launches the app after the app is prepared.
  virtual void LaunchApp();
  // Restarts the installation process.
  virtual void RestartLauncher();

  // Replaces data retriever used for new WebAppInstallTask in tests.
  void SetDataRetrieverFactoryForTesting(
      base::RepeatingCallback<std::unique_ptr<web_app::WebAppDataRetriever>()>
          data_retriever_factory);

  // Replaces default browser window with |window| during launch.
  void SetBrowserWindowForTesting(BrowserWindow* window);

  // Replaces current |url_loader_| with one provided.
  void SetUrlLoaderForTesting(
      std::unique_ptr<web_app::WebAppUrlLoader> url_loader);

 private:
  void OnAppDataObtained(std::unique_ptr<WebApplicationInfo> app_info);

  const WebKioskAppData* GetCurrentApp() const;

  bool is_installed_ = false;  // Whether the installation was completed.
  Profile* const profile_;
  Delegate* const delegate_;  // Not owned. Owns us.
  const AccountId account_id_;

  Browser* browser_ = nullptr;  // Browser instance that runs the web kiosk app.

  std::unique_ptr<web_app::WebAppInstallTask>
      install_task_;  // task that is used to install the app.
  std::unique_ptr<web_app::WebAppUrlLoader>
      url_loader_;  // Loads the app to be installed.

  // Produces retrievers used to obtain app data during installation.
  base::RepeatingCallback<std::unique_ptr<web_app::WebAppDataRetriever>()>
      data_retriever_factory_;

  BrowserWindow* test_browser_window_ = nullptr;

  base::WeakPtrFactory<WebKioskAppLauncher> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebKioskAppLauncher);
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
