// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "apps/app_lifetime_monitor.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class AppWindow;
class Extension;
}  // namespace extensions

namespace apps {

// This app shim handler that handles events for app shims that correspond to an
// extension.
class ExtensionAppShimHandler : public AppShimHandler,
                                public AppShimHost::Client,
                                public content::NotificationObserver,
                                public AppLifetimeMonitor::Observer,
                                public BrowserListObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual base::FilePath GetFullProfilePath(
        const base::FilePath& relative_path);
    virtual bool ProfileExistsForPath(const base::FilePath& path);
    virtual Profile* ProfileForPath(const base::FilePath& path);
    virtual void LoadProfileAsync(const base::FilePath& path,
                                  base::OnceCallback<void(Profile*)> callback);
    virtual bool IsProfileLockedForPath(const base::FilePath& path);

    virtual extensions::AppWindowRegistry::AppWindowList GetWindows(
        Profile* profile,
        const std::string& extension_id);

    virtual const extensions::Extension* MaybeGetAppExtension(
        content::BrowserContext* context,
        const std::string& extension_id);
    virtual bool AllowShimToConnect(Profile* profile,
                                    const extensions::Extension* extension);
    virtual std::unique_ptr<AppShimHost> CreateHost(
        AppShimHost::Client* client,
        Profile* profile,
        const extensions::Extension* extension);
    virtual void EnableExtension(Profile* profile,
                                 const std::string& extension_id,
                                 base::OnceCallback<void()> callback);
    virtual void LaunchApp(Profile* profile,
                           const extensions::Extension* extension,
                           const std::vector<base::FilePath>& files);
    virtual void LaunchShim(Profile* profile,
                            const extensions::Extension* extension,
                            bool recreate_shims,
                            ShimLaunchedCallback launched_callback,
                            ShimTerminatedCallback terminated_callback);
    virtual void LaunchUserManager();

    virtual void MaybeTerminate();
  };

  // Helper function to get the instance on the browser process. This will be
  // non-null except for tests.
  static ExtensionAppShimHandler* Get();

  ExtensionAppShimHandler();
  ~ExtensionAppShimHandler() override;

  // Get the host corresponding to a profile and app id, or null if there is
  // none.
  AppShimHost* FindHost(Profile* profile, const std::string& app_id);

  // Return the host corresponding to |profile| and |app_id|, if one exists.
  // If one does not exist, create one, and launch the app shim (so that the
  // app shim will connect to the returned host).
  AppShimHost* FindOrCreateHost(Profile* profile,
                                const extensions::Extension* extension);

  // Get the AppShimHost corresponding to a browser instance, returning nullptr
  // if none should exist. If no AppShimHost exists, but one should exist
  // (e.g, because the app process is still launching), create one, which will
  // bind to the app process when it finishes launching.
  AppShimHost* GetHostForBrowser(Browser* browser);

  void SetHostedAppHidden(Profile* profile,
                          const std::string& app_id,
                          bool hidden);

  static const extensions::Extension* MaybeGetAppExtension(
      content::BrowserContext* context,
      const std::string& extension_id);

  static const extensions::Extension* MaybeGetAppForBrowser(Browser* browser);

  void QuitAppForWindow(extensions::AppWindow* app_window);
  void QuitHostedAppForWindow(Profile* profile, const std::string& app_id);
  void HideAppForWindow(extensions::AppWindow* app_window);
  void HideHostedApp(Profile* profile, const std::string& app_id);
  void FocusAppForWindow(extensions::AppWindow* app_window);

  // Instructs the shim to set it's "Hide/Show" state to not-hidden.
  void UnhideWithoutActivationForWindow(extensions::AppWindow* app_window);

  // Instructs the shim to request user attention. Returns false if there is no
  // shim for this window.
  void RequestUserAttentionForWindow(extensions::AppWindow* app_window,
                                     AppShimAttentionType attention_type);

  // Called by AppControllerMac when Chrome hides.
  void OnChromeWillHide();

  // AppShimHandler:
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override;

  // AppShimHost::Client:
  void OnShimLaunchRequested(
      AppShimHost* host,
      bool recreate_shims,
      base::OnceCallback<void(base::Process)> launched_callback,
      base::OnceClosure terminated_callback) override;
  void OnShimProcessDisconnected(AppShimHost* host) override;
  void OnShimFocus(AppShimHost* host,
                   AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& files) override;
  void OnShimSetHidden(AppShimHost* host, bool hidden) override;
  void OnShimQuit(AppShimHost* host) override;

  // AppLifetimeMonitor::Observer overrides:
  void OnAppStart(content::BrowserContext* context,
                  const std::string& app_id) override;
  void OnAppActivated(content::BrowserContext* context,
                      const std::string& app_id) override;
  void OnAppDeactivated(content::BrowserContext* context,
                        const std::string& app_id) override;
  void OnAppStop(content::BrowserContext* context,
                 const std::string& app_id) override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // BrowserListObserver overrides;
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 protected:
  typedef std::set<Browser*> BrowserSet;
  typedef std::map<std::pair<Profile*, std::string>, BrowserSet> AppBrowserMap;

  // Virtual for tests.
  virtual bool IsAcceptablyCodeSigned(pid_t pid) const;

  // Exposed for testing.
  void set_delegate(Delegate* delegate);
  content::NotificationRegistrar& registrar() { return registrar_; }

 private:
  // Gets the extension for the corresponding |host|. Note that extensions can
  // be uninstalled at any time (even between sending OnAppClosed() to the host,
  // and receiving the quit confirmation). If the extension has been uninstalled
  // or disabled, the host is immediately closed. If non-nil, the Extension's
  // Profile will be set in |profile|.
  const extensions::Extension* MaybeGetExtensionOrCloseHost(AppShimHost* host,
                                                            Profile** profile);

  // Closes all browsers associated with an app.
  void CloseBrowsersForApp(Profile* profile, const std::string& app_id);

  // Close all app shims associated with the specified profile.
  void CloseShimsForProfile(Profile* profile);

  // Close one specified app.
  void CloseShimForApp(Profile* profile, const std::string& app_id);

  // This is passed to Delegate::LoadProfileAsync for shim-initiated launches
  // where the profile was not yet loaded.
  void OnProfileLoaded(std::unique_ptr<AppShimHostBootstrap> bootstrap,
                       Profile* profile);

  // This is passed to Delegate::EnableViaPrompt for shim-initiated launches
  // where the extension is disabled.
  void OnExtensionEnabled(std::unique_ptr<AppShimHostBootstrap> bootstrap);

  std::unique_ptr<Delegate> delegate_;

  // The state for an individual (app, Profile) pair. This includes the
  // AppShimHost.
  // TODO(https://crbug.com/982024): Add browser windows.
  struct ProfileState {
    ProfileState();
    ProfileState(ProfileState&& other);
    ProfileState& operator=(ProfileState&& other);
    ~ProfileState();

    std::unique_ptr<AppShimHost> host;

   private:
    DISALLOW_COPY_AND_ASSIGN(ProfileState);
  };

  // The state for an individual app. This includes the state for all
  // profiles that are using the app.
  // TODO(https://crbug.com/982024): Add AppShimHost when shared amongst
  // profiles.
  struct AppState {
    AppState();
    AppState(AppState&& other);
    AppState& operator=(AppState&& other);
    ~AppState();

    std::map<Profile*, ProfileState> profiles;

   private:
    DISALLOW_COPY_AND_ASSIGN(AppState);
  };

  // Map from extension id to the state for that app.
  std::map<std::string, AppState> apps_;

  // A map of app ids to associated browser windows.
  AppBrowserMap app_browser_windows_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ExtensionAppShimHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppShimHandler);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_
