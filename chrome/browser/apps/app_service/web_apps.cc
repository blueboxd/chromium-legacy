// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/arc/arc_service_manager.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "url/url_constants.h"

namespace {

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

apps::AppLaunchParams CreateAppLaunchParamsForIntent(
    const std::string& app_id,
    const apps::mojom::IntentPtr& intent) {
  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::mojom::AppLaunchSource::kSourceNone);

  if (intent->scheme.has_value() && intent->host.has_value() &&
      intent->path.has_value()) {
    params.source = apps::mojom::AppLaunchSource::kSourceIntentUrl;
    params.override_url =
        GURL(intent->scheme.value() + url::kStandardSchemeSeparator +
             intent->host.value() + intent->path.value());
    DCHECK(params.override_url.is_valid());
  }

  return params;
}

apps::mojom::InstallSource GetHighestPriorityInstallSource(
    const web_app::WebApp* web_app) {
  switch (web_app->GetHighestPrioritySource()) {
    case web_app::Source::kSystem:
      return apps::mojom::InstallSource::kSystem;
    case web_app::Source::kPolicy:
      return apps::mojom::InstallSource::kPolicy;
    case web_app::Source::kWebAppStore:
      return apps::mojom::InstallSource::kUser;
    case web_app::Source::kSync:
      return apps::mojom::InstallSource::kUser;
    case web_app::Source::kDefault:
      return apps::mojom::InstallSource::kDefault;
  }
}

}  // namespace

namespace apps {

WebApps::WebApps(const mojo::Remote<apps::mojom::AppService>& app_service,
                 Profile* profile)
    : profile_(profile),
      provider_(web_app::WebAppProvider::Get(profile)),
      app_service_(nullptr) {
  Initialize(app_service);
}

WebApps::~WebApps() {
  // In unit tests, AppServiceProxy might be ReInitializeForTesting, so
  // WebApps might be destroyed without calling Shutdown, so arc_prefs_
  // needs to be removed from observer in the destructor function.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

void WebApps::FlushMojoCallsForTesting() {
  receiver_.FlushForTesting();
}

void WebApps::Shutdown() {
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }

  if (profile_) {
    content_settings_observer_.RemoveAll();
  }
}

void WebApps::ObserveArc() {
  // Observe the ARC apps to set the badge on the equivalent web app's icon.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
  }

  arc_prefs_ = ArcAppListPrefs::Get(profile_);
  if (arc_prefs_) {
    arc_prefs_->AddObserver(this);
  }
}

void WebApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  DCHECK(provider_);
  app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
                                 apps::mojom::AppType::kWeb);

  registrar_observer_.Add(&provider_->registrar());
  content_settings_observer_.Add(
      HostContentSettingsMapFactory::GetForProfile(profile_));
  app_service_ = app_service.get();
}

const web_app::WebApp* WebApps::GetWebApp(const web_app::AppId& app_id) const {
  return GetRegistrar().GetAppById(app_id);
}

const web_app::WebAppRegistrar& WebApps::GetRegistrar() const {
  DCHECK(provider_);

  // TODO(loyso): Remove this downcast after bookmark apps erasure.
  web_app::WebAppSyncBridge* sync_bridge =
      provider_->registry_controller().AsWebAppSyncBridge();
  DCHECK(sync_bridge);
  return sync_bridge->registrar();
}

void WebApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebApps::StartPublishingWebApps,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(subscriber_remote)));
}

void WebApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconCompression icon_compression,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  DCHECK(provider_);

  if (icon_key) {
    LoadIconFromWebApp(
        provider_->icon_manager(), icon_compression, size_hint_in_dip, app_id,
        static_cast<IconEffects>(icon_key->icon_effects), std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void WebApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     int64_t display_id) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  // TODO(loyso): Record UMA_HISTOGRAM_ENUMERATION here based on launch_source.

  web_app::DisplayMode display_mode =
      GetRegistrar().GetAppEffectiveDisplayMode(app_id);
  apps::mojom::LaunchContainer fallback_container =
      apps::mojom::LaunchContainer::kLaunchContainerNone;

  switch (display_mode) {
    case web_app::DisplayMode::kBrowser:
      fallback_container = apps::mojom::LaunchContainer::kLaunchContainerTab;
      break;
    case web_app::DisplayMode::kMinimalUi:
      fallback_container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
      break;
    case web_app::DisplayMode::kStandalone:
      fallback_container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
      break;
    case web_app::DisplayMode::kFullscreen:
      fallback_container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
      break;
    case web_app::DisplayMode::kUndefined:
      break;
  }

  AppLaunchParams params = CreateAppIdLaunchParamsWithEventFlags(
      web_app->app_id(), event_flags,
      apps::mojom::AppLaunchSource::kSourceAppLauncher, display_id,
      fallback_container);

  // The app will be created for the currently active profile.
  apps::LaunchService::Get(profile_)->OpenApplication(params);
}

void WebApps::LaunchAppWithIntent(const std::string& app_id,
                                  apps::mojom::IntentPtr intent,
                                  apps::mojom::LaunchSource launch_source,
                                  int64_t display_id) {
  if (!profile_) {
    return;
  }

  AppLaunchParams params = CreateAppLaunchParamsForIntent(app_id, intent);

  apps::LaunchService::Get(profile_)->OpenApplication(params);
}

void WebApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = web_app->launch_url();

  ContentSettingsType permission_type =
      static_cast<ContentSettingsType>(permission->permission_id);
  if (!base::Contains(kSupportedPermissionTypes, permission_type)) {
    return;
  }

  DCHECK_EQ(permission->value_type,
            apps::mojom::PermissionValueType::kTriState);
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (static_cast<apps::mojom::TriState>(permission->value)) {
    case apps::mojom::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::mojom::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::mojom::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, /*resource_identifier=*/std::string(),
      permission_value);
}

void WebApps::PromptUninstall(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  web_app::WebAppUiManagerImpl::Get(profile_)->dialog_manager().UninstallWebApp(
      app_id, web_app::WebAppDialogManager::UninstallSource::kAppMenu,
      /*parent_window=*/nullptr, base::DoNothing());
}

void WebApps::Uninstall(const std::string& app_id,
                        bool clear_site_data,
                        bool report_abuse) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  DCHECK(provider_);
  DCHECK(provider_->install_finalizer().CanUserUninstallExternalApp(app_id));

  provider_->install_finalizer().UninstallExternalAppByUser(app_id,
                                                            base::DoNothing());

  if (!clear_site_data) {
    // TODO(loyso): Add UMA_HISTOGRAM_ENUMERATION here.
    return;
  }

  // TODO(loyso): Add UMA_HISTOGRAM_ENUMERATION here.
  constexpr bool kClearCookies = true;
  constexpr bool kClearStorage = true;
  constexpr bool kClearCache = true;
  constexpr bool kAvoidClosingConnections = false;
  content::ClearSiteData(base::BindRepeating(
                             [](content::BrowserContext* browser_context) {
                               return browser_context;
                             },
                             base::Unretained(profile_)),
                         url::Origin::Create(web_app->launch_url()),
                         kClearCookies, kClearStorage, kClearCache,
                         kAvoidClosingConnections, base::DoNothing());
}

void WebApps::PauseApp(const std::string& app_id) {
  if (paused_apps_.find(app_id) != paused_apps_.end()) {
    return;
  }

  paused_apps_.insert(app_id);
  SetIconEffect(app_id);

  // TODO(crbug.com/1011235): If the app is running, Stop the app.
}

void WebApps::UnpauseApps(const std::string& app_id) {
  if (paused_apps_.find(app_id) == paused_apps_.end()) {
    return;
  }

  paused_apps_.erase(app_id);
  SetIconEffect(app_id);
}

void WebApps::OpenNativeSettings(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  chrome::ShowSiteSettings(profile_, web_app->launch_url());
}

void WebApps::OnPreferredAppSet(const std::string& app_id,
                                apps::mojom::IntentFilterPtr intent_filter,
                                apps::mojom::IntentPtr intent) {
  NOTIMPLEMENTED();
}

void WebApps::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  // If content_type is not one of the supported permissions, do nothing.
  if (!base::Contains(kSupportedPermissionTypes, content_type)) {
    return;
  }

  if (!profile_) {
    return;
  }

  for (const web_app::WebApp& web_app : GetRegistrar().AllApps()) {
    if (web_app.is_in_sync_install()) {
      continue;
    }

    if (primary_pattern.Matches(web_app.launch_url())) {
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = apps::mojom::AppType::kWeb;
      app->app_id = web_app.app_id();
      PopulatePermissions(&web_app, &app->permissions);

      Publish(std::move(app));
    }
  }
}

void WebApps::OnWebAppInstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    Publish(Convert(web_app, apps::mojom::Readiness::kReady));
  }
}

void WebApps::OnWebAppUninstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  paused_apps_.erase(web_app->app_id());

  // Construct an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = web_app->app_id();
  // TODO(loyso): Plumb uninstall source (reason) here.
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;

  SetShowInFields(app, web_app);
  Publish(std::move(app));

  if (app_service_) {
    app_service_->RemovePreferredApp(apps::mojom::AppType::kWeb,
                                     web_app->app_id());
  }
}

void WebApps::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

void WebApps::Publish(apps::mojom::AppPtr app) {
  for (auto& subscriber : subscribers_) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  }
}

void WebApps::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ApplyChromeBadge(package_info.package_name);
}

void WebApps::OnPackageRemoved(const std::string& package_name,
                               bool uninstalled) {
  ApplyChromeBadge(package_name);
}

void WebApps::OnPackageListInitialRefreshed() {
  if (!arc_prefs_) {
    return;
  }

  for (const auto& app_name : arc_prefs_->GetPackagesFromPrefs()) {
    ApplyChromeBadge(app_name);
  }
}

void WebApps::OnArcAppListPrefsDestroyed() {
  arc_prefs_ = nullptr;
}

void WebApps::SetShowInFields(apps::mojom::AppPtr& app,
                              const web_app::WebApp* web_app) {
  auto show = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_management =
      web_app->IsSystemApp() ? show : apps::mojom::OptionalBool::kFalse;
}

void WebApps::PopulatePermissions(const web_app::WebApp* web_app,
                                  std::vector<mojom::PermissionPtr>* target) {
  const GURL url = web_app->launch_url();

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting = host_content_settings_map->GetContentSetting(
        url, url, type, /*resource_identifier=*/std::string());

    // Map ContentSettingsType to an apps::mojom::TriState value
    apps::mojom::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::mojom::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::mojom::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::mojom::TriState::kBlock;
        break;
      default:
        setting_val = apps::mojom::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, std::string(),
                                                 &setting_info);

    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(type);
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(setting_val);
    permission->is_managed =
        setting_info.source == content_settings::SETTING_SOURCE_POLICY;

    target->push_back(std::move(permission));
  }
}

void WebApps::PopulateIntentFilters(
    const base::Optional<GURL>& app_scope,
    std::vector<mojom::IntentFilterPtr>* target) {
  if (app_scope != base::nullopt) {
    target->push_back(
        apps_util::CreateIntentFilterForUrlScope(app_scope.value()));
  }
}

apps::mojom::AppPtr WebApps::Convert(const web_app::WebApp* web_app,
                                     apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = web_app->app_id();
  app->readiness = readiness;
  app->name = web_app->name();
  app->short_name = web_app->name();
  app->description = web_app->description();
  app->icon_key = icon_key_factory_.MakeIconKey(GetIconEffects(web_app));
  // app->version is left empty here.
  // TODO(loyso): Populate app->last_launch_time and app->install_time.

  PopulatePermissions(web_app, &app->permissions);

  app->install_source = GetHighestPriorityInstallSource(web_app);

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->paused = apps::mojom::OptionalBool::kFalse;
  SetShowInFields(app, web_app);

  // Get the intent filters for PWAs.
  PopulateIntentFilters(GetRegistrar().GetAppScope(web_app->app_id()),
                        &app->intent_filters);

  return app;
}

void WebApps::ConvertWebApps(apps::mojom::Readiness readiness,
                             std::vector<apps::mojom::AppPtr>* apps_out) {
  for (const web_app::WebApp& web_app : GetRegistrar().AllApps()) {
    if (!web_app.is_in_sync_install()) {
      apps_out->push_back(Convert(&web_app, readiness));
    }
  }
}

void WebApps::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(apps::mojom::Readiness::kReady, &apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

IconEffects WebApps::GetIconEffects(const web_app::WebApp* web_app) {
  IconEffects icon_effects = IconEffects::kNone;
#if defined(OS_CHROMEOS)
  icon_effects =
      static_cast<IconEffects>(icon_effects | IconEffects::kResizeAndPad);
  if (extensions::util::ShouldApplyChromeBadgeToWebApp(profile_,
                                                       web_app->app_id())) {
    icon_effects = static_cast<IconEffects>(icon_effects | IconEffects::kBadge);
  }
#endif
  if (!web_app->is_locally_installed()) {
    icon_effects = static_cast<IconEffects>(icon_effects | IconEffects::kGray);
  }
  icon_effects =
      static_cast<IconEffects>(icon_effects | IconEffects::kRoundCorners);
  if (paused_apps_.find(web_app->app_id()) != paused_apps_.end()) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
  }
  return icon_effects;
}

void WebApps::ApplyChromeBadge(const std::string& package_name) {
  const std::vector<std::string> app_ids =
      extensions::util::GetEquivalentInstalledAppIds(package_name);

  for (auto& app_id : app_ids) {
    if (GetWebApp(app_id)) {
      SetIconEffect(app_id);
    }
  }
}

void WebApps::SetIconEffect(const std::string& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(GetIconEffects(web_app));
  Publish(std::move(app));
}

}  // namespace apps
