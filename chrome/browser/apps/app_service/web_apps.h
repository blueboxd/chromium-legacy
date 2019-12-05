// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace web_app {
class WebApp;
class WebAppProvider;
class WebAppRegistrar;
}  // namespace web_app

namespace apps {

// An app publisher (in the App Service sense) of Web Apps.
class WebApps : public apps::mojom::Publisher,
                public web_app::AppRegistrarObserver,
                public content_settings::Observer,
                public ArcAppListPrefs::Observer {
 public:
  WebApps(const mojo::Remote<apps::mojom::AppService>& app_service,
          Profile* profile);
  WebApps(const WebApps&) = delete;
  WebApps& operator=(const WebApps&) = delete;
  ~WebApps() override;

  void FlushMojoCallsForTesting();

  void Shutdown();

  void ObserveArc();

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  const web_app::WebApp* GetWebApp(const web_app::AppId& app_id) const;
  const web_app::WebAppRegistrar& GetRegistrar() const;

  // apps::mojom::Publisher overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
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
  void LaunchAppWithIntent(const std::string& app_id,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void PromptUninstall(const std::string& app_id) override;
  void Uninstall(const std::string& app_id,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApps(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void OnPreferredAppSet(const std::string& app_id,
                         apps::mojom::IntentFilterPtr intent_filter,
                         apps::mojom::IntentPtr intent) override;

  // content_settings::Observer overrides.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // web_app::AppRegistrarObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnWebAppUninstalled(const web_app::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;
  // TODO(loyso): Implement app->last_launch_time field for the new system.

  // ArcAppListPrefs::Observer overrides.
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;
  void OnArcAppListPrefsDestroyed() override;

  void Publish(apps::mojom::AppPtr app);

  void SetShowInFields(apps::mojom::AppPtr& app,
                       const web_app::WebApp* web_app);
  void PopulatePermissions(const web_app::WebApp* web_app,
                           std::vector<mojom::PermissionPtr>* target);
  void PopulateIntentFilters(const base::Optional<GURL>& app_scope,
                             std::vector<mojom::IntentFilterPtr>* target);
  apps::mojom::AppPtr Convert(const web_app::WebApp* web_app,
                              apps::mojom::Readiness readiness);
  void ConvertWebApps(apps::mojom::Readiness readiness,
                      std::vector<apps::mojom::AppPtr>* apps_out);
  void StartPublishingWebApps(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote);

  IconEffects GetIconEffects(const web_app::WebApp* web_app);

  // Get the equivalent Chrome app from |arc_package_name| and set the Chrome
  // app badge on the icon effects for the equivalent Chrome apps. If the
  // equivalent ARC app is installed, add the Chrome app badge, otherwise,
  // remove the Chrome app badge.
  void ApplyChromeBadge(const std::string& arc_package_name);

  void SetIconEffect(const std::string& app_id);

  mojo::Receiver<apps::mojom::Publisher> receiver_{this};
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;

  ScopedObserver<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      registrar_observer_{this};

  ScopedObserver<HostContentSettingsMap, content_settings::Observer>
      content_settings_observer_{this};

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  std::set<std::string> paused_apps_;

  web_app::WebAppProvider* const provider_;

  ArcAppListPrefs* arc_prefs_ = nullptr;

  // app_service_ is owned by the object that owns this object.
  apps::mojom::AppService* app_service_;

  base::WeakPtrFactory<WebApps> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_H_
