// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

void SetIcons(const WebApplicationInfo& web_app_info, WebApp* web_app) {
  WebApp::Icons web_app_icons;

  for (const WebApplicationInfo::IconInfo& icon_info : web_app_info.icons) {
    // Skip unfetched bitmaps.
    if (icon_info.data.colorType() == kUnknown_SkColorType)
      continue;

    DCHECK_EQ(icon_info.width, icon_info.height);

    WebApp::IconInfo web_app_icon_info;
    web_app_icon_info.url = icon_info.url;
    web_app_icon_info.size_in_px = icon_info.width;

    web_app_icons.push_back(web_app_icon_info);
  }

  web_app->SetIcons(std::move(web_app_icons));
}

}  // namespace

WebAppInstallFinalizer::WebAppInstallFinalizer(WebAppIconManager* icon_manager)
    : icon_manager_(icon_manager) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AppId app_id = GenerateAppIdFromURL(web_app_info.app_url);
  if (registrar_->GetAppById(app_id)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), app_id,
                                  InstallResultCode::kAlreadyInstalled));
    return;
  }

  auto web_app = std::make_unique<WebApp>(app_id);

  web_app->SetName(base::UTF16ToUTF8(web_app_info.title));
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app->SetLaunchUrl(web_app_info.app_url);
  web_app->SetScope(web_app_info.scope);
  web_app->SetThemeColor(web_app_info.theme_color);
  web_app->SetLaunchContainer(web_app_info.open_as_window
                                  ? LaunchContainer::kWindow
                                  : LaunchContainer::kTab);

  SetIcons(web_app_info, web_app.get());

  icon_manager_->WriteData(
      std::move(app_id), std::make_unique<WebApplicationInfo>(web_app_info),
      base::BindOnce(&WebAppInstallFinalizer::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(web_app)));
}

void WebAppInstallFinalizer::UninstallExternalWebApp(
    const GURL& app_url,
    UninstallExternalWebAppCallback callback) {
  NOTIMPLEMENTED();
}

void WebAppInstallFinalizer::OnDataWritten(InstallFinalizedCallback callback,
                                           std::unique_ptr<WebApp> web_app,
                                           bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  AppId app_id = web_app->app_id();

  registrar_->RegisterApp(std::move(web_app));
  // TODO(loyso): NotifyWebAppInstalled should be a part of RegisterApp.
  registrar_->NotifyWebAppInstalled(app_id);

  std::move(callback).Run(std::move(app_id), InstallResultCode::kSuccess);
}

bool WebAppInstallFinalizer::CanCreateOsShortcuts() const {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
  return false;
}

void WebAppInstallFinalizer::CreateOsShortcuts(
    const AppId& app_id,
    bool add_to_desktop,
    CreateOsShortcutsCallback callback) {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), false /* shortcuts_created */));
}

bool WebAppInstallFinalizer::CanRevealAppShim() const {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
  return false;
}

void WebAppInstallFinalizer::RevealAppShim(const AppId& app_id) {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
}

bool WebAppInstallFinalizer::CanSkipAppUpdateForSync(
    const AppId& app_id,
    const WebApplicationInfo& web_app_info) const {
  NOTIMPLEMENTED();
  return true;
}

void WebAppInstallFinalizer::SetSubsystems(AppRegistrar* registrar,
                                           WebAppUiManager* ui_manager) {
  registrar_ = registrar ? registrar->AsWebAppRegistrar() : nullptr;
  InstallFinalizer::SetSubsystems(registrar, ui_manager);
}

}  // namespace web_app
