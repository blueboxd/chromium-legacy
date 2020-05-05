// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace web_app {

struct ShortcutInfo;

// This class manages creation/update/deletion of OS shortcuts for web
// applications.
//
// TODO(crbug.com/860581): Migrate functions from
// web_app_extension_shortcut.(h|cc) and
// platform_apps/shortcut_manager.(h|cc) to web_app::AppShortcutManager and
// its subclasses.
class AppShortcutManager : public AppRegistrarObserver {
 public:
  explicit AppShortcutManager(Profile* profile);
  ~AppShortcutManager() override;

  void SetSubsystems(AppRegistrar* registrar);

  void Start();
  void Shutdown();

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;

  // Tells the AppShortcutManager that no shortcuts should actually be written
  // to the disk.
  void SuppressShortcutsForTesting();

  // virtual for testing.
  virtual bool CanCreateShortcuts() const;
  // virtual for testing.
  virtual void CreateShortcuts(const AppId& app_id,
                               bool add_to_desktop,
                               CreateShortcutsCallback callback);

  // Registers a shortcuts menu for the web app's icon with the OS.
  void RegisterShortcutsMenuWithOs(
      const std::vector<WebApplicationShortcutInfo>& shortcuts,
      const AppId& app_id);

  // TODO(https://crbug.com/1069306): Implement UnregisterShortcutsMenuWithOS()
  // to support local offline installs and uninstalls.

  // The result of a call to GetShortcutInfo.
  using GetShortcutInfoCallback =
      base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;
  // Asynchronously gets the information required to create a shortcut for
  // |app_id| including all the icon bitmaps. Returns nullptr if app_id is
  // uninstalled or becomes uninstalled during the asynchronous read of icons.
  virtual void GetShortcutInfoForApp(const AppId& app_id,
                                     GetShortcutInfoCallback callback) = 0;

 protected:
  void DeleteSharedAppShims(const AppId& app_id);
  void OnShortcutsCreated(const AppId& app_id,
                          CreateShortcutsCallback callback,
                          bool success);

  AppRegistrar* registrar() { return registrar_; }
  Profile* profile() { return profile_; }

 private:
  void OnShortcutInfoRetrievedCreateShortcuts(
      bool add_to_desktop,
      CreateShortcutsCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  ScopedObserver<AppRegistrar, AppRegistrarObserver> app_registrar_observer_{
      this};

  bool suppress_shortcuts_for_testing_ = false;

  AppRegistrar* registrar_ = nullptr;
  Profile* const profile_;

  base::WeakPtrFactory<AppShortcutManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppShortcutManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
