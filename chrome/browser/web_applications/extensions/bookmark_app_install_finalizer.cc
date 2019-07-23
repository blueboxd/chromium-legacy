// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bookmark_app_extension_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const Extension* GetExtensionById(Profile* profile,
                                  const web_app::AppId& app_id) {
  const Extension* app =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(app_id);
  DCHECK(app);
  return app;
}

void OnExtensionInstalled(
    const GURL& app_url,
    LaunchType launch_type,
    bool is_locally_installed,
    web_app::InstallFinalizer::InstallFinalizedCallback callback,
    scoped_refptr<CrxInstaller> crx_installer,
    const base::Optional<CrxInstallError>& error) {
  if (error) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kFailedUnknownReason);
    return;
  }

  const Extension* extension = crx_installer->extension();
  DCHECK(extension);
  if (extension !=
      GetExtensionById(crx_installer->profile(), extension->id())) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kWebAppDisabled);
    return;
  }

  DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension), app_url);

  SetLaunchType(crx_installer->profile(), extension->id(), launch_type);

  SetBookmarkAppIsLocallyInstalled(crx_installer->profile(), extension,
                                   is_locally_installed);

  std::move(callback).Run(extension->id(),
                          web_app::InstallResultCode::kSuccess);
}

}  // namespace

BookmarkAppInstallFinalizer::BookmarkAppInstallFinalizer(Profile* profile)
    : profile_(profile), externally_installed_app_prefs_(profile->GetPrefs()) {
  crx_installer_factory_ = base::BindRepeating([](Profile* profile) {
    ExtensionService* extension_service =
        ExtensionSystem::Get(profile)->extension_service();
    DCHECK(extension_service);
    return CrxInstaller::CreateSilent(extension_service);
  });
}

BookmarkAppInstallFinalizer::~BookmarkAppInstallFinalizer() = default;

void BookmarkAppInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  scoped_refptr<CrxInstaller> crx_installer =
      crx_installer_factory_.Run(profile_);

  extensions::LaunchType launch_type =
      web_app_info.open_as_window ? LAUNCH_TYPE_WINDOW : LAUNCH_TYPE_REGULAR;

  // Override extensions::LaunchType with force web_app::LaunchContainer:
  switch (options.force_launch_container) {
    case web_app::LaunchContainer::kDefault:
      // force_launch_container is not defined, do not override.
      break;
    case web_app::LaunchContainer::kTab:
      launch_type = LAUNCH_TYPE_REGULAR;
      break;
    case web_app::LaunchContainer::kWindow:
      launch_type = LAUNCH_TYPE_WINDOW;
      break;
  }

  crx_installer->set_installer_callback(base::BindOnce(
      OnExtensionInstalled, web_app_info.app_url, launch_type,
      options.locally_installed, std::move(callback), crx_installer));

  switch (options.install_source) {
      // TODO(nigeltao/ortuno): should these two cases lead to different
      // Manifest::Location values: INTERNAL vs EXTERNAL_PREF_DOWNLOAD?
    case WebappInstallSource::INTERNAL_DEFAULT:
    case WebappInstallSource::EXTERNAL_DEFAULT:
      crx_installer->set_install_source(Manifest::EXTERNAL_PREF_DOWNLOAD);
      // CrxInstaller::InstallWebApp will OR the creation flags with
      // FROM_BOOKMARK.
      crx_installer->set_creation_flags(Extension::WAS_INSTALLED_BY_DEFAULT);
      break;
    case WebappInstallSource::EXTERNAL_POLICY:
      crx_installer->set_install_source(Manifest::EXTERNAL_POLICY_DOWNLOAD);
      break;
    case WebappInstallSource::SYSTEM_DEFAULT:
      // System Apps are considered EXTERNAL_COMPONENT as they are downloaded
      // from the WebUI they point to. COMPONENT seems like the more correct
      // value, but usages (icon loading, filesystem cleanup), are tightly
      // coupled to this value, making it unsuitable.
      crx_installer->set_install_source(Manifest::EXTERNAL_COMPONENT);
      // InstallWebApp will OR the creation flags with FROM_BOOKMARK.
      crx_installer->set_creation_flags(Extension::WAS_INSTALLED_BY_DEFAULT);
      break;
    case WebappInstallSource::COUNT:
      NOTREACHED();
      break;
    default:
      // All other install sources mean user-installed app. Do nothing.
      break;
  }

  if (options.no_network_install) {
    // Ensure that this app is not synced. A no-network install means we have
    // all data locally, so assume that there is some mechanism to propagate
    // the local source of data in place of usual extension sync.
    crx_installer->set_install_source(Manifest::EXTERNAL_PREF_DOWNLOAD);
  }

  crx_installer->InstallWebApp(web_app_info);
}

void BookmarkAppInstallFinalizer::UninstallExternalWebApp(
    const GURL& app_url,
    UninstallExternalWebAppCallback callback) {
  base::Optional<web_app::AppId> app_id =
      externally_installed_app_prefs_.LookupAppId(app_url);
  if (!app_id.has_value()) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url
                 << "; No corresponding extension for url.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (!ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
          app_id.value())) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url
                 << "; Extension not installed.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  base::string16 error;
  bool uninstalled =
      ExtensionSystem::Get(profile_)->extension_service()->UninstallExtension(
          app_id.value(), UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, &error);

  if (!uninstalled) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url << ". "
                 << error;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), uninstalled));
}

bool BookmarkAppInstallFinalizer::CanCreateOsShortcuts() const {
  return CanBookmarkAppCreateOsShortcuts();
}

void BookmarkAppInstallFinalizer::CreateOsShortcuts(
    const web_app::AppId& app_id,
    bool add_to_desktop,
    CreateOsShortcutsCallback callback) {
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppCreateOsShortcuts(profile_, app, add_to_desktop,
                               std::move(callback));
}

bool BookmarkAppInstallFinalizer::CanPinAppToShelf() const {
  return CanBookmarkAppBePinnedToShelf();
}

void BookmarkAppInstallFinalizer::PinAppToShelf(const web_app::AppId& app_id) {
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppPinToShelf(app);
}

bool BookmarkAppInstallFinalizer::CanReparentTab(const web_app::AppId& app_id,
                                                 bool shortcut_created) const {
  const Extension* app = GetExtensionById(profile_, app_id);
  return CanBookmarkAppReparentTab(profile_, app, shortcut_created);
}

void BookmarkAppInstallFinalizer::ReparentTab(
    const web_app::AppId& app_id,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  DCHECK(!profile_->IsOffTheRecord());
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppReparentTab(web_contents, app);
}

bool BookmarkAppInstallFinalizer::CanRevealAppShim() const {
  return CanBookmarkAppRevealAppShim();
}

void BookmarkAppInstallFinalizer::RevealAppShim(const web_app::AppId& app_id) {
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppRevealAppShim(profile_, app);
}

bool BookmarkAppInstallFinalizer::CanSkipAppUpdateForSync(
    const web_app::AppId& app_id,
    const WebApplicationInfo& web_app_info) const {
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(extension_service);

  const Extension* extension = extension_service->GetInstalledExtension(app_id);
  if (!extension)
    return false;

  // We can skip if there are no bookmark app details that need updating.
  // TODO(loyso): We need to check more data fields. crbug.com/949427.
  const std::string extension_sync_data_name =
      base::UTF16ToUTF8(web_app_info.title);
  const std::string bookmark_app_description =
      base::UTF16ToUTF8(web_app_info.description);
  if (extension->non_localized_name() == extension_sync_data_name &&
      extension->description() == bookmark_app_description) {
    return true;
  }

  return false;
}

void BookmarkAppInstallFinalizer::SetCrxInstallerFactoryForTesting(
    CrxInstallerFactory crx_installer_factory) {
  crx_installer_factory_ = crx_installer_factory;
}

}  // namespace extensions
