// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_

#include "ash/public/cpp/android_intent_helper.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

class DeviceActions : public ash::AndroidIntentHelper,
                      public chromeos::assistant::mojom::DeviceActions,
                      public ArcAppListPrefs::Observer {
 public:
  DeviceActions();
  ~DeviceActions() override;

  chromeos::assistant::mojom::DeviceActionsPtr AddBinding();

  // mojom::DeviceActions overrides:
  void SetWifiEnabled(bool enabled) override;
  void SetBluetoothEnabled(bool enabled) override;
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override;
  void SetScreenBrightnessLevel(double level, bool gradual) override;
  void SetNightLightEnabled(bool enabled) override;
  void OpenAndroidApp(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                      OpenAndroidAppCallback callback) override;
  void VerifyAndroidApp(
      std::vector<chromeos::assistant::mojom::AndroidAppInfoPtr> apps_info,
      VerifyAndroidAppCallback callback) override;
  void LaunchAndroidIntent(const std::string& intent) override;
  void AddAppListEventSubscriber(
      chromeos::assistant::mojom::AppListEventSubscriberPtr subscriber)
      override;

 private:
  // ArcAppListPrefs::Observer overrides.
  void OnPackageListInitialRefreshed() override;
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& id) override;

  ScopedObserver<ArcAppListPrefs, DeviceActions> scoped_prefs_observer_;
  mojo::BindingSet<chromeos::assistant::mojom::DeviceActions> bindings_;
  mojo::InterfacePtrSet<chromeos::assistant::mojom::AppListEventSubscriber>
      app_list_subscribers_;
  DISALLOW_COPY_AND_ASSIGN(DeviceActions);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_
