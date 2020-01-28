// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

class Profile;

namespace web_app {

class AppShortcutObserver;
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

  void AddObserver(AppShortcutObserver* observer);
  void RemoveObserver(AppShortcutObserver* observer);

  // AppRegistrarObserver:
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;

  // Tells the AppShortcutManager that no shortcuts should actually be written
  // to the disk.
  void SuppressShortcutsForTesting();

  // virtual for testing.
  virtual bool CanCreateShortcuts() const;
  // virtual for testing.
  virtual void CreateShortcuts(const AppId& app_id,
                               bool add_to_desktop,
                               CreateShortcutsCallback callback);

  // Builds initial ShortcutInfo without |ShortcutInfo::favicon| being read.
  virtual std::unique_ptr<ShortcutInfo> BuildShortcutInfo(
      const AppId& app_id) = 0;

  // The result of a call to GetShortcutInfo.
  using GetShortcutInfoCallback =
      base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;
  // Asynchronously gets the information required to create a shortcut for
  // |app_id|.
  virtual void GetShortcutInfoForApp(const AppId& app_id,
                                     GetShortcutInfoCallback callback) = 0;

 protected:
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

  base::ObserverList<AppShortcutObserver, /*check_empty=*/true> observers_;

  bool suppress_shortcuts_for_testing_ = false;

  AppRegistrar* registrar_ = nullptr;
  Profile* const profile_;

  base::WeakPtrFactory<AppShortcutManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppShortcutManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
