// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// Forward declarations of generalized interfaces.
class PendingAppManager;
class InstallManager;
class InstallFinalizer;
class WebAppAudioFocusIdMap;
class WebAppTabHelperBase;
class SystemWebAppManager;
class AppRegistrar;
class WebAppUiManager;

// Forward declarations for new extension-independent subsystems.
class WebAppDatabase;
class WebAppDatabaseFactory;
class WebAppIconManager;
class WebAppSyncManager;

// Forward declarations for legacy extension-based subsystems.
class WebAppPolicyManager;

// Connects Web App features, such as the installation of default and
// policy-managed web apps, with Profiles (as WebAppProvider is a
// Profile-linked KeyedService) and their associated PrefService.
//
// Lifecycle notes:
// All subsystems are constructed independently of each other in the
// WebAppProvider constructor.
// Subsystem construction should have no side effects and start no tasks.
// Tests can replace any of the subsystems before Start() is called.
// Similarly, in destruction, subsystems should not refer to each other.
class WebAppProvider : public WebAppProviderBase,
                       public content::NotificationObserver {
 public:
  static WebAppProvider* Get(Profile* profile);
  static WebAppProvider* GetForWebContents(content::WebContents* web_contents);

  explicit WebAppProvider(Profile* profile);
  ~WebAppProvider() override;

  // Start the Web App system. This will run subsystem startup tasks.
  void Start();

  // WebAppProviderBase:
  AppRegistrar& registrar() override;
  InstallManager& install_manager() override;
  PendingAppManager& pending_app_manager() override;
  WebAppPolicyManager* policy_manager() override;
  WebAppUiManager& ui_manager() override;

  WebAppDatabaseFactory& database_factory() { return *database_factory_; }
  WebAppSyncManager& sync_manager() { return *sync_manager_; }

  // KeyedService:
  void Shutdown() override;

  SystemWebAppManager& system_web_app_manager();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static WebAppTabHelperBase* CreateTabHelper(
      content::WebContents* web_contents);

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Signals when app registry becomes ready.
  const base::OneShotEvent& on_registry_ready() const {
    return on_registry_ready_;
  }

 protected:
  virtual void StartImpl();

  // Create extension-independent subsystems.
  void CreateWebAppsSubsystems(Profile* profile);
  // ... or create legacy extension-based subsystems.
  void CreateBookmarkAppsSubsystems(Profile* profile);

  // Wire together subsystems but do not start them (yet).
  void ConnectSubsystems();

  // Start registry. All other subsystems depend on it.
  void StartRegistry();
  void OnRegistryReady();

  void OnScanForExternalWebApps(std::vector<ExternalInstallOptions>);

  // Called just before profile destruction. All WebContents must be destroyed
  // by the end of this method.
  void ProfileDestroyed();

  // New extension-independent subsystems:
  std::unique_ptr<WebAppAudioFocusIdMap> audio_focus_id_map_;
  std::unique_ptr<WebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppDatabase> database_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppSyncManager> sync_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;

  // New generalized subsystems:
  std::unique_ptr<AppRegistrar> registrar_;
  std::unique_ptr<InstallFinalizer> install_finalizer_;
  std::unique_ptr<InstallManager> install_manager_;
  std::unique_ptr<PendingAppManager> pending_app_manager_;
  std::unique_ptr<SystemWebAppManager> system_web_app_manager_;

  // Legacy extension-based subsystems:
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;

  content::NotificationRegistrar notification_registrar_;

  base::OneShotEvent on_registry_ready_;

  Profile* profile_;

  // Ensures that ConnectSubsystems() is not called after Start().
  bool started_ = false;
  bool connected_ = false;

  base::WeakPtrFactory<WebAppProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppProvider);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
