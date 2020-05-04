// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps_chromeos.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Get the LaunchId for a given |app_window|. Set launch_id default value to an
// empty string. If show_in_shelf parameter is true and the window key is not
// empty, its value is appended to the launch_id. Otherwise, if the window key
// is empty, the session_id is used.
std::string GetLaunchId(extensions::AppWindow* app_window) {
  std::string launch_id;
  if (app_window->show_in_shelf()) {
    if (!app_window->window_key().empty()) {
      launch_id = app_window->window_key();
    } else {
      launch_id = base::StringPrintf("%d", app_window->session_id().id());
    }
  }
  return launch_id;
}

}  // namespace

namespace apps {

ExtensionAppsChromeOs::ExtensionAppsChromeOs(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile,
    apps::mojom::AppType app_type,
    apps::InstanceRegistry* instance_registry)
    : ExtensionAppsBase(app_service, profile, app_type),
      instance_registry_(instance_registry) {
  DCHECK(instance_registry_);
  Initialize();
}

ExtensionAppsChromeOs::~ExtensionAppsChromeOs() {
  app_window_registry_.RemoveAll();

  // In unit tests, AppServiceProxy might be ReInitializeForTesting, so
  // ExtensionApps might be destroyed without calling Shutdown, so arc_prefs_
  // needs to be removed from observer in the destructor function.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

// static
void ExtensionAppsChromeOs::RecordUninstallCanceledAction(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (!extension) {
    return;
  }

  if (extension->from_bookmark()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Webapp.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_CANCELED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_CANCELED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
  }
}

void ExtensionAppsChromeOs::Shutdown() {
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

void ExtensionAppsChromeOs::ObserveArc() {
  // Observe the ARC apps to set the badge on the equivalent Chrome app's icon.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
  }

  arc_prefs_ = ArcAppListPrefs::Get(profile());
  if (arc_prefs_) {
    arc_prefs_->AddObserver(this);
  }
}

void ExtensionAppsChromeOs::Initialize() {
  app_window_registry_.Add(extensions::AppWindowRegistry::Get(profile()));
  notification_display_service_.Add(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  // Remaining initialization is only relevant to the kExtension app type.
  if (app_type() != apps::mojom::AppType::kExtension) {
    return;
  }

  profile_pref_change_registrar_.Init(profile()->GetPrefs());
  profile_pref_change_registrar_.Add(
      prefs::kHideWebStoreIcon,
      base::Bind(&ExtensionAppsBase::OnHideWebStoreIconPrefChanged,
                 GetWeakPtr()));
}

void ExtensionAppsChromeOs::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  auto* tab = LaunchAppWithIntentImpl(app_id, event_flags, std::move(intent),
                                      launch_source, display_id);

  if (launch_source == apps::mojom::LaunchSource::kFromArc && tab) {
    // Add a flag to remember this tab originated in the ARC context.
    tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                     std::make_unique<arc::ArcWebContentsData>());
  }
}

void ExtensionAppsChromeOs::Uninstall(const std::string& app_id,
                                      bool clear_site_data,
                                      bool report_abuse) {
  // TODO(crbug.com/1009248): We need to add the error code, which could be used
  // by ExtensionFunction, ManagementUninstallFunctionBase on the callback
  // OnExtensionUninstallDialogClosed
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionRegistry::Get(profile())->GetInstalledExtension(
          app_id);
  if (!extension.get()) {
    return;
  }

  base::string16 error;
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->UninstallExtension(app_id, extensions::UNINSTALL_REASON_USER_INITIATED,
                           &error);

  if (extension->from_bookmark()) {
    if (!clear_site_data) {
      UMA_HISTOGRAM_ENUMERATION(
          "Webapp.UninstallDialogAction",
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_UNINSTALL,
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
      return;
    }

    UMA_HISTOGRAM_ENUMERATION(
        "Webapp.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::
            CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);

    constexpr bool kClearCookies = true;
    constexpr bool kClearStorage = true;
    constexpr bool kClearCache = true;
    constexpr bool kAvoidClosingConnections = false;
    content::ClearSiteData(
        base::BindRepeating(
            [](content::BrowserContext* browser_context) {
              return browser_context;
            },
            base::Unretained(profile())),
        url::Origin::Create(
            extensions::AppLaunchInfo::GetFullLaunchURL(extension.get())),
        kClearCookies, kClearStorage, kClearCache, kAvoidClosingConnections,
        base::DoNothing());
  } else {
    if (!report_abuse) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.UninstallDialogAction",
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_UNINSTALL,
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
      return;
    }

    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::
            CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);

    // If the extension specifies a custom uninstall page via
    // chrome.runtime.setUninstallURL, then at uninstallation its uninstall
    // page opens. To ensure that the CWS Report Abuse page is the active
    // tab at uninstallation, navigates to the url to report abuse.
    constexpr char kReferrerId[] = "chrome-remove-extension-dialog";
    NavigateParams params(
        profile(),
        extension_urls::GetWebstoreReportAbuseUrl(app_id, kReferrerId),
        ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
}

void ExtensionAppsChromeOs::PauseApp(const std::string& app_id) {
  paused_apps_.MaybeAddApp(app_id);
  constexpr bool kPaused = true;
  Publish(paused_apps_.GetAppWithPauseStatus(app_type(), app_id, kPaused),
          subscribers());

  SetIconEffect(app_id);

  if (instance_registry_->GetWindows(app_id).empty()) {
    return;
  }

  // For Web apps that are opened in app windows, close all tabs to close the
  // opened window, otherwise, show pause information in browsers.
  bool is_web_app = false;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_app()) {
      continue;
    }
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id) {
      TabStripModel* tab_strip = browser->tab_strip_model();
      tab_strip->CloseAllTabs();
      is_web_app = true;
    }
  }

  // For web apps that are opened in tabs, PauseApp() should be
  // called with Chrome's app_id to show pause information in browsers.
  if (is_web_app) {
    return;
  }

  chromeos::app_time::AppTimeLimitInterface* app_limit =
      chromeos::app_time::AppTimeLimitInterface::Get(profile());
  DCHECK(app_limit);
  app_limit->PauseWebActivity(app_id);
}

void ExtensionAppsChromeOs::UnpauseApps(const std::string& app_id) {
  paused_apps_.MaybeRemoveApp(app_id);
  constexpr bool kPaused = false;
  Publish(paused_apps_.GetAppWithPauseStatus(app_type(), app_id, kPaused),
          subscribers());

  SetIconEffect(app_id);

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_app()) {
      continue;
    }
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id) {
      return;
    }
  }

  chromeos::app_time::AppTimeLimitInterface* app_time =
      chromeos::app_time::AppTimeLimitInterface::Get(profile());
  DCHECK(app_time);
  app_time->ResumeWebActivity(app_id);
}

void ExtensionAppsChromeOs::GetMenuModel(const std::string& app_id,
                                         apps::mojom::MenuType menu_type,
                                         int64_t display_id,
                                         GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  if (app_id == extension_misc::kChromeAppId) {
    GetMenuModelForChromeBrowserApp(menu_type, std::move(callback));
    return;
  }

  bool is_platform_app = extension->is_platform_app();
  bool is_system_web_app = web_app::WebAppProvider::Get(profile())
                               ->system_web_app_manager()
                               .IsSystemWebApp(app_id);

  if (!is_platform_app && !is_system_web_app) {
    CreateOpenNewSubmenu(
        menu_type,
        extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile()),
                                  extension) ==
                extensions::LaunchType::LAUNCH_TYPE_WINDOW
            ? IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW
            : IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
        &menu_items);
  }

  if (!is_platform_app && menu_type == apps::mojom::MenuType::kAppList &&
      extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile()) &&
      extensions::OptionsPageInfo::HasOptionsPage(extension)) {
    AddCommandItem(ash::OPTIONS, IDS_NEW_TAB_APP_OPTIONS, &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      !instance_registry_->GetWindows(app_id).empty()) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile())->management_policy();
  DCHECK(policy);
  if (policy->UserMayModifySettings(extension, nullptr) &&
      !policy->MustRemainInstalled(extension, nullptr)) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  if (!is_system_web_app && extension->ShouldDisplayInAppLauncher()) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void ExtensionAppsChromeOs::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  DCHECK(!instance_registry_->Exists(app_window->GetNativeWindow()));
  app_window_to_aura_window_[app_window] = app_window->GetNativeWindow();

  // Attach window to multi-user manager now to let it manage visibility state
  // of the window correctly.
  if (SessionControllerClientImpl::IsMultiProfileAvailable()) {
    auto* multi_user_window_manager =
        MultiUserWindowManagerHelper::GetWindowManager();
    if (multi_user_window_manager) {
      multi_user_window_manager->SetWindowOwner(
          app_window->GetNativeWindow(),
          multi_user_util::GetAccountIdFromProfile(profile()));
    }
  }
  RegisterInstance(app_window, InstanceState::kStarted);
}

void ExtensionAppsChromeOs::OnAppWindowShown(extensions::AppWindow* app_window,
                                             bool was_hidden) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  InstanceState state =
      instance_registry_->GetState(app_window->GetNativeWindow());

  // If the window is shown, it should be started, running and not hidden.
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state =
      static_cast<apps::InstanceState>(state & ~apps::InstanceState::kHidden);
  RegisterInstance(app_window, state);
}

void ExtensionAppsChromeOs::OnAppWindowHidden(
    extensions::AppWindow* app_window) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  // For hidden |app_window|, the other state bit, started, running, active, and
  // visible should be cleared.
  RegisterInstance(app_window, InstanceState::kHidden);
}

void ExtensionAppsChromeOs::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  RegisterInstance(app_window, InstanceState::kDestroyed);
  app_window_to_aura_window_.erase(app_window);
}

void ExtensionAppsChromeOs::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (!Accepts(extension)) {
    return;
  }

  paused_apps_.MaybeRemoveApp(extension->id());

  ExtensionAppsBase::OnExtensionUninstalled(browser_context, extension, reason);
}

void ExtensionAppsChromeOs::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ApplyChromeBadge(package_info.package_name);
}

void ExtensionAppsChromeOs::OnPackageRemoved(const std::string& package_name,
                                             bool uninstalled) {
  ApplyChromeBadge(package_name);
}

void ExtensionAppsChromeOs::OnPackageListInitialRefreshed() {
  if (!arc_prefs_) {
    return;
  }
  for (const auto& app_name : arc_prefs_->GetPackagesFromPrefs()) {
    ApplyChromeBadge(app_name);
  }
}

void ExtensionAppsChromeOs::OnArcAppListPrefsDestroyed() {
  arc_prefs_ = nullptr;
}

void ExtensionAppsChromeOs::OnNotificationDisplayed(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  switch (notification.notifier_id().type) {
    case message_center::NotifierType::APPLICATION:
      MaybeAddNotification(notification.notifier_id().id, notification.id());
      return;
    case message_center::NotifierType::WEB_PAGE:
      MaybeAddWebPageNotifications(notification, metadata);
      return;
    default:
      return;
  }
}

void ExtensionAppsChromeOs::OnNotificationClosed(
    const std::string& notification_id) {
  const std::string& app_id =
      app_notifications_.GetAppIdForNotification(notification_id);
  if (app_id.empty() || MaybeGetExtension(app_id) == nullptr) {
    return;
  }
  app_notifications_.RemoveNotification(notification_id);
  Publish(app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id),
          subscribers());
}

void ExtensionAppsChromeOs::OnNotificationDisplayServiceDestroyed(
    NotificationDisplayService* service) {
  notification_display_service_.Remove(service);
}

void ExtensionAppsChromeOs::MaybeAddNotification(
    const std::string& app_id,
    const std::string& notification_id) {
  if (MaybeGetExtension(app_id) == nullptr) {
    return;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  Publish(app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id),
          subscribers());
}

void ExtensionAppsChromeOs::MaybeAddWebPageNotifications(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  const GURL& url =
      metadata
          ? PersistentNotificationMetadata::From(metadata)->service_worker_scope
          : notification.origin_url();

  if (app_type() == apps::mojom::AppType::kExtension) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile());
    DCHECK(registry);
    const extensions::ExtensionSet& extensions = registry->enabled_extensions();
    const extensions::Extension* extension = extensions.GetAppByURL(url);
    if (extension) {
      MaybeAddNotification(extension->id(), notification.id());
    }
    return;
  }

  if (metadata) {
    // For persistent notifications, find the web app with the scope url.
    base::Optional<web_app::AppId> app_id =
        web_app::FindInstalledAppWithUrlInScope(profile(), url,
                                                /*window_only=*/false);
    if (app_id.has_value()) {
      MaybeAddNotification(app_id.value(), notification.id());
    }
  } else {
    // For non-persistent notifications, find all web apps that are installed
    // under the origin url.
    auto* web_app_provider = web_app::WebAppProvider::Get(profile());
    if (!web_app_provider) {
      return;
    }

    auto app_ids = web_app_provider->registrar().FindAppsInScope(url);
    for (const auto& app_id : app_ids) {
      MaybeAddNotification(app_id, notification.id());
    }
  }
}

// static
bool ExtensionAppsChromeOs::IsBlacklisted(const std::string& app_id) {
  // We blacklist (meaning we don't publish the app, in the App Service sense)
  // some apps that are already published by other app publishers.
  //
  // This sense of "blacklist" is separate from the extension registry's
  // kDisabledByBlacklist concept, which is when SafeBrowsing will send out a
  // blacklist of malicious extensions to disable.

  // The Play Store is conceptually provided by the ARC++ publisher, but
  // because it (the Play Store icon) is also the UI for enabling Android apps,
  // we also want to show the app icon even before ARC++ is enabled. Prior to
  // the App Service, as a historical implementation quirk, the Play Store both
  // has an "ARC++ app" component and an "Extension app" component, and both
  // share the same App ID.
  //
  // In the App Service world, there should be a unique app publisher for any
  // given app. In this case, the ArcApps publisher publishes the Play Store
  // app, and the ExtensionApps publisher does not.
  return app_id == arc::kPlayStoreAppId;
}

void ExtensionAppsChromeOs::UpdateShowInFields(const std::string& app_id) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type();
  app->app_id = app_id;
  SetShowInFields(app, extension);
  Publish(std::move(app), subscribers());
}

void ExtensionAppsChromeOs::OnHideWebStoreIconPrefChanged() {
  UpdateShowInFields(extensions::kWebStoreAppId);
  UpdateShowInFields(extension_misc::kEnterpriseWebStoreAppId);
}

bool ExtensionAppsChromeOs::Accepts(const extensions::Extension* extension) {
  if (!extension->is_app() || IsBlacklisted(extension->id())) {
    return false;
  }

  switch (app_type()) {
    case apps::mojom::AppType::kExtension:
      return !extension->from_bookmark();
    case apps::mojom::AppType::kWeb:
      // Crostini Terminal System App is handled by Crostini Apps.
      // TODO(crbug.com/1028898): Register Terminal as a System App rather than
      // a crostini app.
      if (extension->id() == crostini::kCrostiniTerminalSystemAppId) {
        return false;
      }
      return extension->from_bookmark();
    default:
      NOTREACHED();
      return false;
  }
}

bool ExtensionAppsChromeOs::ShouldShownInLauncher(
    const extensions::Extension* extension) {
  return app_list::ShouldShowInLauncher(extension, profile());
}

apps::mojom::AppPtr ExtensionAppsChromeOs::Convert(
    const extensions::Extension* extension,
    apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = ConvertImpl(extension, readiness);

  bool paused = paused_apps_.IsPaused(extension->id());
  app->icon_key =
      icon_key_factory_.MakeIconKey(GetIconEffects(extension, paused));

  app->has_badge = app_notifications_.HasNotification(extension->id())
                       ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
  app->paused = paused ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;

  return app;
}

IconEffects ExtensionAppsChromeOs::GetIconEffects(
    const extensions::Extension* extension,
    bool paused) {
  IconEffects icon_effects = IconEffects::kNone;
  icon_effects =
      static_cast<IconEffects>(icon_effects | IconEffects::kResizeAndPad);
  if (extensions::util::ShouldApplyChromeBadge(profile(), extension->id())) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kChromeBadge);
  }
  icon_effects = static_cast<IconEffects>(
      icon_effects | ExtensionAppsBase::GetIconEffects(extension));
  if (paused) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
  }
  return icon_effects;
}

void ExtensionAppsChromeOs::ApplyChromeBadge(const std::string& package_name) {
  const std::vector<std::string> extension_ids =
      extensions::util::GetEquivalentInstalledExtensions(profile(),
                                                         package_name);

  for (auto& app_id : extension_ids) {
    SetIconEffect(app_id);
  }
}

void ExtensionAppsChromeOs::SetIconEffect(const std::string& app_id) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type();
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(
      GetIconEffects(extension, paused_apps_.IsPaused(app_id)));
  Publish(std::move(app), subscribers());
}

bool ExtensionAppsChromeOs::ShouldRecordAppWindowActivity(
    extensions::AppWindow* app_window) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry)) {
    return false;
  }

  DCHECK(app_window);

  const extensions::Extension* extension = app_window->GetExtension();
  if (!extension) {
    return false;
  }

  // ARC Play Store is not published by this publisher, but the window for Play
  // Store should be able to be added to InstanceRegistry.
  if (extension->id() == arc::kPlayStoreAppId &&
      app_type() == apps::mojom::AppType::kExtension) {
    return true;
  }

  if (!Accepts(extension)) {
    return false;
  }

  return true;
}

void ExtensionAppsChromeOs::RegisterInstance(extensions::AppWindow* app_window,
                                             InstanceState new_state) {
  aura::Window* window = app_window->GetNativeWindow();

  // If the current state has been marked as |new_state|, we don't need to
  // update.
  if (instance_registry_->GetState(window) == new_state) {
    return;
  }

  if (new_state == InstanceState::kDestroyed) {
    DCHECK(base::Contains(app_window_to_aura_window_, app_window));
    window = app_window_to_aura_window_[app_window];
  }
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  auto instance =
      std::make_unique<apps::Instance>(app_window->extension_id(), window);
  instance->SetLaunchId(GetLaunchId(app_window));
  instance->UpdateState(new_state, base::Time::Now());
  instance->SetBrowserContext(app_window->browser_context());
  deltas.push_back(std::move(instance));
  instance_registry_->OnInstances(deltas);
}

void ExtensionAppsChromeOs::GetMenuModelForChromeBrowserApp(
    apps::mojom::MenuType menu_type,
    GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  // "Normal" windows are not allowed when incognito is enforced.
  if (IncognitoModePrefs::GetAvailability(profile()->GetPrefs()) !=
      IncognitoModePrefs::FORCED) {
    AddCommandItem((menu_type == apps::mojom::MenuType::kAppList)
                       ? ash::APP_CONTEXT_MENU_NEW_WINDOW
                       : ash::MENU_NEW_WINDOW,
                   IDS_APP_LIST_NEW_WINDOW, &menu_items);
  }

  // Incognito windows are not allowed when incognito is disabled.
  if (!profile()->IsOffTheRecord() &&
      IncognitoModePrefs::GetAvailability(profile()->GetPrefs()) !=
          IncognitoModePrefs::DISABLED) {
    AddCommandItem((menu_type == apps::mojom::MenuType::kAppList)
                       ? ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW
                       : ash::MENU_NEW_INCOGNITO_WINDOW,
                   IDS_APP_LIST_NEW_INCOGNITO_WINDOW, &menu_items);
  }

  AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                 &menu_items);

  std::move(callback).Run(std::move(menu_items));
}

}  // namespace apps
