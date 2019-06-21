// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

class Profile;

namespace apps {

class AppServiceProxy;

// An app publisher (in the App Service sense) of ARC++ apps,
//
// See chrome/services/app_service/README.md.
class ArcApps : public KeyedService,
                public apps::mojom::Publisher,
                public arc::ConnectionObserver<arc::mojom::AppInstance>,
                public ArcAppListPrefs::Observer {
 public:
  using AppConnectionHolder =
      arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>;

  static ArcApps* Get(Profile* profile);

  static ArcApps* CreateForTesting(Profile* profile,
                                   apps::AppServiceProxy* proxy);

  explicit ArcApps(Profile* profile);

  ~ArcApps() override;

 private:
  ArcApps(Profile* profile, apps::AppServiceProxy* proxy);

  // apps::mojom::Publisher overrides.
  void Connect(apps::mojom::SubscriberPtr subscriber,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // arc::ConnectionObserver<arc::mojom::AppInstance> overrides.
  void OnConnectionReady() override;

  // ArcAppListPrefs::Observer overrides.
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnAppIconUpdated(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor) override;
  void OnAppNameUpdated(const std::string& app_id,
                        const std::string& name) override;
  void OnAppLastLaunchTimeUpdated(const std::string& app_id) override;
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageListInitialRefreshed() override;

  const base::FilePath GetCachedIconFilePath(const std::string& app_id,
                                             int32_t size_hint_in_dip);
  void LoadIconFromVM(const std::string app_id,
                      apps::mojom::IconCompression icon_compression,
                      int32_t size_hint_in_dip,
                      bool allow_placeholder_icon,
                      IconEffects icon_effects,
                      LoadIconCallback callback);
  void LoadPlayStoreIcon(apps::mojom::IconCompression icon_compression,
                         int32_t size_hint_in_dip,
                         IconEffects icon_effects,
                         LoadIconCallback callback);

  apps::mojom::AppPtr Convert(const std::string& app_id,
                              const ArcAppListPrefs::AppInfo& app_info);
  void Publish(apps::mojom::AppPtr app);
  void ConvertAndPublishPackageApps(
      const arc::mojom::ArcPackageInfo& package_info);

  mojo::Binding<apps::mojom::Publisher> binding_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;
  ArcAppListPrefs* prefs_;

  std::vector<base::OnceCallback<void(AppConnectionHolder*)>>
      pending_load_icon_calls_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  base::WeakPtrFactory<ArcApps> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_H_
