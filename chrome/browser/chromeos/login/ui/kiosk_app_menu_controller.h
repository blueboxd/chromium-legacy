// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_KIOSK_APP_MENU_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_KIOSK_APP_MENU_CONTROLLER_H_

#include "ash/public/cpp/kiosk_app_menu.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"

namespace chromeos {

// Observer class to update the Kiosk app menu when Kiosk app data is changed.
class KioskAppMenuController
    : public KioskAppManagerObserver,
      public ArcKioskAppManager::ArcKioskAppManagerObserver,
      public WebKioskAppManager::WebKioskAppManagerObserver {
 public:
  KioskAppMenuController();
  ~KioskAppMenuController() override;

  // Manually dispatch kiosk app data to Ash.
  void SendKioskApps();

  // KioskAppManagerObserver:
  void OnKioskAppDataChanged(const std::string& app_id) override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) override;
  void OnKioskAppsSettingsChanged() override;

  // ArcKioskAppManagerObserver:
  void OnArcKioskAppsChanged() override;

  // WebKioskAppManagerObserver:
  void OnWebKioskAppsChanged() override;

 private:
  void LaunchApp(const ash::KioskAppMenuEntry& app);

  ScopedObserver<KioskAppManager, KioskAppManagerObserver> kiosk_observer_{
      this};
  ScopedObserver<ArcKioskAppManager,
                 ArcKioskAppManager::ArcKioskAppManagerObserver>
      arc_kiosk_observer_{this};
  ScopedObserver<WebKioskAppManager, KioskAppMenuController>
      web_kiosk_observer_{this};

  base::WeakPtrFactory<KioskAppMenuController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(KioskAppMenuController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_KIOSK_APP_MENU_CONTROLLER_H_
