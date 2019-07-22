// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"

enum class WebappInstallSource;
struct WebApplicationInfo;

namespace content {
class WebContents;
}

class Profile;

namespace web_app {

enum class InstallResultCode;
class InstallManagerObserver;
class InstallFinalizer;
class AppRegistrar;
struct ExternalInstallOptions;

// TODO(loyso): Rework this interface once BookmarkAppHelper erased. Unify the
// API and merge similar InstallWebAppZZZZ functions. crbug.com/915043.
class InstallManager {
 public:
  using OnceInstallCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;

  // Callback used to indicate whether a user has accepted the installation of a
  // web app.
  using WebAppInstallationAcceptanceCallback =
      base::OnceCallback<void(bool user_accepted,
                              std::unique_ptr<WebApplicationInfo>)>;

  // Callback to show the WebApp installation confirmation bubble in UI.
  // |web_app_info| is the WebApplicationInfo to be installed.
  using WebAppInstallDialogCallback = base::OnceCallback<void(
      content::WebContents* initiator_web_contents,
      std::unique_ptr<WebApplicationInfo> web_app_info,
      ForInstallableSite for_installable_site,
      WebAppInstallationAcceptanceCallback acceptance_callback)>;

  using WebAppInstallabilityCheckCallback =
      base::OnceCallback<void(std::unique_ptr<content::WebContents>,
                              bool is_installable)>;

  // Returns true if a web app can be installed for a given |web_contents|.
  virtual bool CanInstallWebApp(content::WebContents* web_contents) = 0;

  // Checks a WebApp installability, retrieves manifest and icons and
  // than performs the actual installation.
  virtual void InstallWebAppFromManifest(
      content::WebContents* web_contents,
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) = 0;

  // Infers WebApp info from the blink renderer process and then retrieves a
  // manifest in a way similar to |InstallWebAppFromManifest|. If the manifest
  // is incomplete or missing, the inferred info is used. |force_shortcut_app|
  // forces the creation of a shortcut app instead of a PWA even if installation
  // is available.
  virtual void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) = 0;

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // InstallManager doesn't fetch a manifest. If |no_network_install| is true,
  // the app will not be synced, since if the data is locally available we
  // assume there is an external sync mechanism.
  virtual void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool no_network_install,
      WebappInstallSource install_source,
      OnceInstallCallback callback) = 0;

  // Starts a background web app installation process for a given
  // |web_contents|.
  virtual void InstallWebAppWithOptions(
      content::WebContents* web_contents,
      const ExternalInstallOptions& install_options,
      OnceInstallCallback callback) = 0;

  // Starts background installation or an update of a web app from the sync
  // system. |web_application_info| contains received sync data. Icons will be
  // downloaded from the icon URLs provided in |web_application_info|.
  virtual void InstallOrUpdateWebAppFromSync(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) = 0;

  // Starts background installation of a web app from the given
  // |web_application_info|.
  virtual void InstallWebAppForTesting(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) = 0;

  explicit InstallManager(Profile* profile);
  virtual ~InstallManager();

  void SetSubsystems(AppRegistrar* registrar, InstallFinalizer* finalizer);

  virtual void Shutdown();

  // Loads |web_app_url| in a new WebContents and determines if it is
  // installable. Returns the WebContents and whether the app is installable or
  // not.
  void LoadWebAppAndCheckInstallability(const GURL& web_app_url,
                                        WebAppInstallabilityCheckCallback);

  void AddObserver(InstallManagerObserver* observer);
  void RemoveObserver(InstallManagerObserver* observer);

 protected:
  Profile* profile() { return profile_; }
  AppRegistrar* registrar() { return registrar_; }
  InstallFinalizer* finalizer() { return finalizer_; }

 private:
  Profile* profile_;
  WebAppUrlLoader url_loader_;

  AppRegistrar* registrar_ = nullptr;
  InstallFinalizer* finalizer_ = nullptr;

  base::ObserverList<InstallManagerObserver, true /*check_empty*/> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
