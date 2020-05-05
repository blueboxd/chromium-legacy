// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <map>
#include <vector>

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

// TODO(loyso): Call sites should specify Source explicitly as a part of
// AppTraits parameter object.
Source::Type InferSourceFromMetricsInstallSource(
    WebappInstallSource install_source) {
  switch (install_source) {
    case WebappInstallSource::MENU_BROWSER_TAB:
    case WebappInstallSource::MENU_CUSTOM_TAB:
    case WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case WebappInstallSource::API_BROWSER_TAB:
    case WebappInstallSource::API_CUSTOM_TAB:
    case WebappInstallSource::DEVTOOLS:
    case WebappInstallSource::MANAGEMENT_API:
    case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case WebappInstallSource::SYNC:
      return Source::kSync;

    case WebappInstallSource::INTERNAL_DEFAULT:
    case WebappInstallSource::EXTERNAL_DEFAULT:
      return Source::kDefault;

    case WebappInstallSource::EXTERNAL_POLICY:
      return Source::kPolicy;

    case WebappInstallSource::SYSTEM_DEFAULT:
      return Source::kSystem;

    case WebappInstallSource::ARC:
      return Source::kWebAppStore;

    case WebappInstallSource::COUNT:
      NOTREACHED();
      return Source::kSync;
  }
}

Source::Type InferSourceFromExternalInstallSource(
    ExternalInstallSource external_install_source) {
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
    case ExternalInstallSource::kExternalDefault:
      return Source::kDefault;

    case ExternalInstallSource::kExternalPolicy:
      return Source::kPolicy;

    case ExternalInstallSource::kSystemInstalled:
      return Source::kSystem;

    case ExternalInstallSource::kArc:
      return Source::kWebAppStore;
  }
}

std::vector<SquareSizePx> GetSquareSizePxs(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  std::vector<SquareSizePx> sizes;
  sizes.reserve(icon_bitmaps.size());
  for (const std::pair<const SquareSizePx, SkBitmap>& item : icon_bitmaps)
    sizes.push_back(item.first);
  return sizes;
}

void SetWebAppFileHandlers(
    const std::vector<blink::Manifest::FileHandler>& manifest_file_handlers,
    WebApp* web_app) {
  apps::FileHandlers web_app_file_handlers;

  for (const auto& manifest_file_handler : manifest_file_handlers) {
    apps::FileHandler web_app_file_handler;
    web_app_file_handler.action = manifest_file_handler.action;

    for (const auto& it : manifest_file_handler.accept) {
      apps::FileHandler::AcceptEntry web_app_accept_entry;
      web_app_accept_entry.mime_type = base::UTF16ToUTF8(it.first);
      for (const auto& manifest_file_extension : it.second)
        web_app_accept_entry.file_extensions.insert(
            base::UTF16ToUTF8(manifest_file_extension));
      web_app_file_handler.accept.push_back(std::move(web_app_accept_entry));
    }

    web_app_file_handlers.push_back(std::move(web_app_file_handler));
  }

  web_app->SetFileHandlers(std::move(web_app_file_handlers));
}

}  // namespace

WebAppInstallFinalizer::WebAppInstallFinalizer(Profile* profile,
                                               WebAppSyncBridge* sync_bridge,
                                               WebAppIconManager* icon_manager)
    : profile_(profile),
      sync_bridge_(sync_bridge),
      icon_manager_(icon_manager) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(loyso): Expose Source argument as a field of AppTraits struct.
  const auto source =
      InferSourceFromMetricsInstallSource(options.install_source);

  const AppId app_id = GenerateAppIdFromURL(web_app_info.app_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);

  std::unique_ptr<WebApp> web_app;

  if (existing_web_app) {
    // There is an existing app from other source(s). Preserve
    // |is_locally_installed| and |user_display_mode| fields here, do not modify
    // them. Prepare copy-on-write:
    DCHECK_EQ(web_app_info.app_url, existing_web_app->launch_url());
    web_app = std::make_unique<WebApp>(*existing_web_app);
  } else {
    // New app.
    web_app = std::make_unique<WebApp>(app_id);
    web_app->SetLaunchUrl(web_app_info.app_url);
    web_app->SetIsLocallyInstalled(options.locally_installed);
    web_app->SetUserDisplayMode(web_app_info.open_as_window
                                    ? DisplayMode::kStandalone
                                    : DisplayMode::kBrowser);
  }

  // `WebApp::chromeos_data` has a default value already. Only override if the
  // caller provided a new value.
  if (options.chromeos_data.has_value())
    web_app->SetWebAppChromeOsData(options.chromeos_data.value());

  web_app->SetAdditionalSearchTerms(web_app_info.additional_search_terms);
  web_app->AddSource(source);
  web_app->SetIsInSyncInstall(false);

  UpdateIntWebAppPref(profile_->GetPrefs(), app_id, kLatestWebAppInstallSource,
                      static_cast<int>(options.install_source));
  SetWebAppManifestFieldsAndWriteData(web_app_info, std::move(web_app),
                                      /*is_new_install=*/true,
                                      std::move(callback));
}

void WebAppInstallFinalizer::FinalizeFallbackInstallAfterSync(
    const AppId& app_id,
    InstallFinalizedCallback callback) {
  const WebApp* app_in_sync_install = GetWebAppRegistrar().GetAppById(app_id);
  DCHECK(app_in_sync_install);
  DCHECK(app_in_sync_install->is_in_sync_install());

  // Promote the app in sync install to a full user-visible app using the poor
  // data that we've got from sync. Prepare copy-on-write:
  auto web_app = std::make_unique<WebApp>(*app_in_sync_install);
  web_app->SetIsInSyncInstall(false);

  web_app->SetName(web_app->sync_data().name);
  web_app->SetThemeColor(web_app->sync_data().theme_color);

  // If no color has been specified, use dark gray.
  const SkColor background_icon_color =
      web_app->sync_data().theme_color.has_value()
          ? web_app->sync_data().theme_color.value()
          : SK_ColorDKGRAY;

  std::map<SquareSizePx, SkBitmap> icon_bitmaps =
      GenerateIcons(web_app->sync_data().name, background_icon_color);
  web_app->SetDownloadedIconSizes(GetSquareSizePxs(icon_bitmaps));

  UpdateIntWebAppPref(profile_->GetPrefs(), app_id, kLatestWebAppInstallSource,
                      static_cast<int>(WebappInstallSource::SYNC));

  InstallFinalizedCallback fallback_install_callback =
      base::BindOnce(&WebAppInstallFinalizer::OnFallbackInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr(),
                     app_in_sync_install->app_id(), std::move(callback));

  icon_manager_->WriteData(
      std::move(app_id), std::move(icon_bitmaps),
      std::vector<std::map<SquareSizePx, SkBitmap>>(),
      base::BindOnce(&WebAppInstallFinalizer::OnIconsDataWritten,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(fallback_install_callback), std::move(web_app),
                     /*is_new_install=*/true));
}

void WebAppInstallFinalizer::FinalizeUninstallAfterSync(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  // WebAppSyncBridge::ApplySyncChangesToRegistrar does the actual
  // NotifyWebAppUninstalled and unregistration of the app from the registry.
  DCHECK(!GetWebAppRegistrar().GetAppById(app_id));

  icon_manager_->DeleteData(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnIconsDataDeleted,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             std::move(callback)));
}

void WebAppInstallFinalizer::UninstallExternalWebApp(
    const AppId& app_id,
    ExternalInstallSource external_install_source,
    UninstallWebAppCallback callback) {
  Source::Type source =
      InferSourceFromExternalInstallSource(external_install_source);
  UninstallWebAppOrRemoveSource(app_id, source, std::move(callback));
}

bool WebAppInstallFinalizer::CanUserUninstallFromSync(
    const AppId& app_id) const {
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  return app ? app->IsSynced() : false;
}

void WebAppInstallFinalizer::UninstallWebAppFromSyncByUser(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  DCHECK(CanUserUninstallFromSync(app_id));
  UninstallWebAppOrRemoveSource(app_id, Source::kSync, std::move(callback));
}

bool WebAppInstallFinalizer::CanUserUninstallExternalApp(
    const AppId& app_id) const {
  // TODO(loyso): Policy Apps: Implement web_app::ManagementPolicy taking
  // extensions::ManagementPolicy::UserMayModifySettings as inspiration.
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  return app ? app->CanUserUninstallExternalApp() : false;
}

void WebAppInstallFinalizer::UninstallExternalAppByUser(
    const AppId& app_id,
    UninstallWebAppCallback callback) {
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  DCHECK(app);
  DCHECK(app->CanUserUninstallExternalApp());

  if (app->IsDefaultApp()) {
    UpdateBoolWebAppPref(profile_->GetPrefs(), app_id,
                         kWasExternalAppUninstalledByUser, true);
  }

  // UninstallExternalAppByUser can wipe out an app with multiple sources. This
  // is the behavior from the old bookmark-app based system, which does not
  // support incremental AddSource/RemoveSource. Here we are preserving that
  // behavior for now.
  // TODO(loyso): Implement different uninstall flows in UI. For example, we
  // should separate UninstallWebAppFromSyncByUser from
  // UninstallExternalAppByUser.
  UninstallWebApp(app_id, std::move(callback));
}

bool WebAppInstallFinalizer::WasExternalAppUninstalledByUser(
    const AppId& app_id) const {
  return GetBoolWebAppPref(profile_->GetPrefs(), app_id,
                           kWasExternalAppUninstalledByUser);
}

void WebAppInstallFinalizer::FinalizeUpdate(
    const WebApplicationInfo& web_app_info,
    InstallFinalizedCallback callback) {
  const AppId app_id = GenerateAppIdFromURL(web_app_info.app_url);
  const WebApp* existing_web_app = GetWebAppRegistrar().GetAppById(app_id);

  if (!existing_web_app || existing_web_app->is_in_sync_install() ||
      web_app_info.app_url != existing_web_app->launch_url()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), AppId(),
                                  InstallResultCode::kWebAppDisabled));
    return;
  }

  // Prepare copy-on-write to update existing app.
  auto web_app = std::make_unique<WebApp>(*existing_web_app);

  SetWebAppManifestFieldsAndWriteData(web_app_info, std::move(web_app),
                                      /*is_new_install=*/false,
                                      std::move(callback));
}

void WebAppInstallFinalizer::UninstallWebApp(const AppId& app_id,
                                             UninstallWebAppCallback callback) {
  registrar().NotifyWebAppUninstalled(app_id);

  // TODO(https://crbug.com/1069306): We should do UnregisterShortcutsMenuWithOs
  // on local uninstall as well.
  if (ShouldRegisterShortcutsMenuWithOs())
    UnregisterShortcutsMenuWithOs(app_id, profile_->GetPath());

  ScopedRegistryUpdate update(sync_bridge_);
  update->DeleteApp(app_id);

  icon_manager_->DeleteData(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnIconsDataDeleted,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             std::move(callback)));
}

void WebAppInstallFinalizer::UninstallWebAppOrRemoveSource(
    const AppId& app_id,
    Source::Type source,
    UninstallWebAppCallback callback) {
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  if (!app) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*uninstalled=*/false));
  }

  if (app->HasOnlySource(source)) {
    UninstallWebApp(app_id, std::move(callback));
  } else {
    ScopedRegistryUpdate update(sync_bridge_);
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->RemoveSource(source);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*uninstalled=*/true));
  }
}

void WebAppInstallFinalizer::SetWebAppManifestFieldsAndWriteData(
    const WebApplicationInfo& web_app_info,
    std::unique_ptr<WebApp> web_app,
    bool is_new_install,
    InstallFinalizedCallback callback) {
  web_app->SetName(base::UTF16ToUTF8(web_app_info.title));
  web_app->SetDisplayMode(web_app_info.display_mode);
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app->SetScope(web_app_info.scope);
  if (web_app_info.theme_color) {
    web_app->SetThemeColor(
        SkColorSetA(*web_app_info.theme_color, SK_AlphaOPAQUE));
  }

  WebApp::SyncData sync_data;
  sync_data.name = base::UTF16ToUTF8(web_app_info.title);
  sync_data.theme_color = web_app_info.theme_color;
  web_app->SetSyncData(std::move(sync_data));

  web_app->SetIconInfos(web_app_info.icon_infos);
  web_app->SetDownloadedIconSizes(GetSquareSizePxs(web_app_info.icon_bitmaps));

  std::vector<WebApp::WebAppShortcutMenuItemInfo> web_app_shortcut_infos;
  std::vector<std::vector<SquareSizePx>> downloaded_shortcut_icons_sizes;
  for (const auto& shortcut : web_app_info.shortcut_infos) {
    WebApp::WebAppShortcutMenuItemInfo shortcut_info;
    shortcut_info.name = shortcut.name;
    shortcut_info.url = shortcut.url;
    shortcut_info.shortcut_icon_infos = shortcut.shortcut_icon_infos;
    web_app_shortcut_infos.push_back(shortcut_info);
    downloaded_shortcut_icons_sizes.push_back(
        GetSquareSizePxs(shortcut.shortcut_icon_bitmaps));
  }
  web_app->SetShortcutInfos(std::move(web_app_shortcut_infos));
  web_app->SetDownloadedShortcutIconsSizes(
      std::move(downloaded_shortcut_icons_sizes));

  SetWebAppFileHandlers(web_app_info.file_handlers, web_app.get());

  AppId app_id = web_app->app_id();

  std::vector<std::map<SquareSizePx, SkBitmap>> shortcut_icons_bitmaps;
  for (const auto& shortcut : web_app_info.shortcut_infos)
    shortcut_icons_bitmaps.push_back(shortcut.shortcut_icon_bitmaps);

  icon_manager_->WriteData(
      std::move(app_id), web_app_info.icon_bitmaps,
      std::move(shortcut_icons_bitmaps),
      base::BindOnce(&WebAppInstallFinalizer::OnIconsDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(web_app), is_new_install));
}

void WebAppInstallFinalizer::OnIconsDataWritten(
    InstallFinalizedCallback callback,
    std::unique_ptr<WebApp> web_app,
    bool is_new_install,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  AppId app_id = web_app->app_id();

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge_->BeginUpdate();

  WebApp* app_to_override = update->UpdateApp(app_id);
  if (app_to_override)
    *app_to_override = std::move(*web_app);
  else
    update->CreateApp(std::move(web_app));

  sync_bridge_->CommitUpdate(
      std::move(update),
      base::BindOnce(&WebAppInstallFinalizer::OnDatabaseCommitCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(app_id), is_new_install));
}

void WebAppInstallFinalizer::OnIconsDataDeleted(
    const AppId& app_id,
    UninstallWebAppCallback callback,
    bool success) {
  std::move(callback).Run(success);
}

void WebAppInstallFinalizer::OnDatabaseCommitCompleted(
    InstallFinalizedCallback callback,
    const AppId& app_id,
    bool is_new_install,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  if (is_new_install)
    registrar().NotifyWebAppInstalled(app_id);

  std::move(callback).Run(
      app_id, is_new_install ? InstallResultCode::kSuccessNewInstall
                             : InstallResultCode::kSuccessAlreadyInstalled);
}

void WebAppInstallFinalizer::OnFallbackInstallFinalized(
    const AppId& app_in_sync_install_id,
    InstallFinalizedCallback callback,
    const AppId& installed_app_id,
    InstallResultCode code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("Webapp.SyncInitiatedFallbackInstallResult", code);
  if (!IsSuccess(code)) {
    DLOG(ERROR) << "Installation failed for app in sync install. app_id="
                << app_in_sync_install_id << " code=" << static_cast<int>(code);
  }

  std::move(callback).Run(installed_app_id, code);
}

WebAppRegistrar& WebAppInstallFinalizer::GetWebAppRegistrar() const {
  WebAppRegistrar* web_app_registrar = registrar().AsWebAppRegistrar();
  DCHECK(web_app_registrar);
  return *web_app_registrar;
}

}  // namespace web_app
