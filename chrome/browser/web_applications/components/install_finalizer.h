// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;

// An abstract finalizer for the installation process, represents the last step.
// Takes WebApplicationInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class InstallFinalizer {
 public:
  using InstallFinalizedCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;
  using UninstallExternalWebAppCallback =
      base::OnceCallback<void(bool uninstalled)>;
  using CreateOsShortcutsCallback =
      base::OnceCallback<void(bool shortcuts_created)>;

  struct FinalizeOptions {
    // If |force_launch_container| defined as non-kDefault then the installed
    // app will launch in |force_launch_container|.
    LaunchContainer force_launch_container = LaunchContainer::kDefault;
    WebappInstallSource install_source = WebappInstallSource::COUNT;
    bool locally_installed = true;
    bool no_network_install = false;
  };

  // Write the WebApp data to disk and register the app.
  virtual void FinalizeInstall(const WebApplicationInfo& web_app_info,
                               const FinalizeOptions& options,
                               InstallFinalizedCallback callback) = 0;

  // Removes the external app for |app_url| from disk and registrar. Fails if
  // there is no installed external app for |app_url|.
  virtual void UninstallExternalWebApp(const GURL& app_url,
                                       UninstallExternalWebAppCallback) = 0;

  virtual bool CanCreateOsShortcuts() const = 0;
  virtual void CreateOsShortcuts(const AppId& app_id,
                                 bool add_to_desktop,
                                 CreateOsShortcutsCallback callback) = 0;

  virtual bool CanPinAppToShelf() const = 0;
  virtual void PinAppToShelf(const AppId& app_id) = 0;

  virtual bool CanReparentTab(const AppId& app_id,
                              bool shortcut_created) const = 0;
  virtual void ReparentTab(const AppId& app_id,
                           content::WebContents* web_contents) = 0;

  virtual bool CanRevealAppShim() const = 0;
  virtual void RevealAppShim(const AppId& app_id) = 0;

  virtual bool CanSkipAppUpdateForSync(
      const AppId& app_id,
      const WebApplicationInfo& web_app_info) const = 0;

  virtual ~InstallFinalizer() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
