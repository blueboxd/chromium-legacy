// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/remote_apps/remote_apps_manager.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

class ImageDownloaderImpl : public RemoteAppsManager::ImageDownloader {
 public:
  ImageDownloaderImpl() = default;
  ImageDownloaderImpl(const ImageDownloaderImpl&) = delete;
  ImageDownloaderImpl& operator=(const ImageDownloaderImpl&) = delete;
  ~ImageDownloaderImpl() override = default;

  void Download(const GURL& url, DownloadCallback callback) override {
    ash::ImageDownloader* image_downloader = ash::ImageDownloader::Get();
    DCHECK(image_downloader);
    // TODO(jityao): Set traffic annotation.
    image_downloader->Download(url, NO_TRAFFIC_ANNOTATION_YET,
                               std::move(callback));
  }
};

}  // namespace

RemoteAppsManager::RemoteAppsManager(Profile* profile)
    : profile_(profile),
      remote_apps_(std::make_unique<apps::RemoteApps>(profile_, this)),
      model_(std::make_unique<RemoteAppsModel>()),
      image_downloader_(std::make_unique<ImageDownloaderImpl>()) {
  app_list_syncable_service_ =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  model_updater_ = app_list_syncable_service_->GetModelUpdater();
  app_list_model_updater_observer_.Add(model_updater_);

  // |AppListSyncableService| manages the Chrome side AppList and has to be
  // initialized before apps can be added.
  if (app_list_syncable_service_->IsInitialized()) {
    Initialize();
  } else {
    app_list_syncable_service_observer_.Add(app_list_syncable_service_);
  }
}

RemoteAppsManager::~RemoteAppsManager() = default;

void RemoteAppsManager::Initialize() {
  DCHECK(app_list_syncable_service_->IsInitialized());
  is_initialized_ = true;
}

void RemoteAppsManager::AddApp(const std::string& name,
                               const std::string& folder_id,
                               const GURL& icon_url,
                               AddAppCallback callback) {
  if (!is_initialized_) {
    std::move(callback).Run(std::string(), Error::kNotReady);
    return;
  }

  if (!folder_id.empty() && !model_->HasFolder(folder_id)) {
    std::move(callback).Run(std::string(), Error::kFolderIdDoesNotExist);
    return;
  }

  const RemoteAppsModel::AppInfo& info =
      model_->AddApp(name, icon_url, folder_id);
  add_app_callback_map_.insert({info.id, std::move(callback)});
  remote_apps_->AddApp(info);
}

RemoteAppsManager::Error RemoteAppsManager::DeleteApp(const std::string& id) {
  // Check if app was added but |HandleOnAppAdded| has not been called.
  if (!model_->HasApp(id) ||
      add_app_callback_map_.find(id) != add_app_callback_map_.end())
    return Error::kAppIdDoesNotExist;

  model_->DeleteApp(id);
  remote_apps_->DeleteApp(id);
  return Error::kNone;
}

std::string RemoteAppsManager::AddFolder(const std::string& folder_name) {
  const RemoteAppsModel::FolderInfo& folder_info =
      model_->AddFolder(folder_name);
  return folder_info.id;
}

RemoteAppsManager::Error RemoteAppsManager::DeleteFolder(
    const std::string& folder_id) {
  if (!model_->HasFolder(folder_id))
    return Error::kFolderIdDoesNotExist;

  // Move all items out of the folder. Empty folders are automatically deleted.
  RemoteAppsModel::FolderInfo& folder_info = model_->GetFolderInfo(folder_id);
  for (const auto& app : folder_info.items)
    model_updater_->MoveItemToFolder(app, std::string());
  model_->DeleteFolder(folder_id);
  return Error::kNone;
}

void RemoteAppsManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void RemoteAppsManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void RemoteAppsManager::Shutdown() {}

const std::map<std::string, RemoteAppsModel::AppInfo>&
RemoteAppsManager::GetApps() {
  return model_->GetAllAppInfo();
}

void RemoteAppsManager::LaunchApp(const std::string& id) {
  for (Observer& observer : observer_list_)
    observer.OnAppLaunched(id);
}

gfx::ImageSkia RemoteAppsManager::GetIcon(const std::string& id) {
  if (!model_->HasApp(id))
    return gfx::ImageSkia();

  return model_->GetAppInfo(id).icon;
}

apps::mojom::MenuItemsPtr RemoteAppsManager::GetMenuModel(
    const std::string& id) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();
  // TODO(jityao): Temporary string for menu item.
  apps::AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                       &menu_items);
  return menu_items;
}

void RemoteAppsManager::OnSyncModelUpdated() {
  DCHECK(!is_initialized_);
  Initialize();
  app_list_syncable_service_observer_.RemoveAll();
}

void RemoteAppsManager::OnAppListItemAdded(ChromeAppListItem* item) {
  if (item->is_folder() || item->is_page_break())
    return;

  // Make a copy of id as item->metadata can be invalidated.
  HandleOnAppAdded(std::string(item->id()));
}

void RemoteAppsManager::SetRemoteAppsForTesting(
    std::unique_ptr<apps::RemoteApps> remote_apps) {
  remote_apps_ = std::move(remote_apps);
}

void RemoteAppsManager::SetImageDownloaderForTesting(
    std::unique_ptr<ImageDownloader> image_downloader) {
  image_downloader_ = std::move(image_downloader);
}

RemoteAppsModel* RemoteAppsManager::GetModelForTesting() {
  return model_.get();
}

void RemoteAppsManager::SetIsInitializedForTesting(bool is_initialized) {
  is_initialized_ = is_initialized;
}

void RemoteAppsManager::HandleOnAppAdded(const std::string& id) {
  if (!model_->HasApp(id))
    return;
  RemoteAppsModel::AppInfo& app_info = model_->GetAppInfo(id);

  const std::string& folder_id = app_info.folder_id;
  // If folder was deleted, |folder_id| would be set to empty by the model, so
  // we don't have to check if it was deleted.
  if (!folder_id.empty()) {
    bool folder_already_exists = model_updater_->FindFolderItem(folder_id);
    model_updater_->MoveItemToFolder(id, folder_id);
    RemoteAppsModel::FolderInfo& folder_info = model_->GetFolderInfo(folder_id);

    if (!folder_already_exists) {
      // Update metadata for newly created folder.
      ChromeAppListItem* item = model_updater_->FindFolderItem(folder_id);
      DCHECK(item) << "Missing folder item for folder_id: " << folder_id;
      item->SetName(folder_info.name);
      item->SetIsPersistent(true);
      item->SetPosition(model_updater_->GetFirstAvailablePosition());
    }
  }

  StartIconDownload(id, app_info.icon_url);

  auto it = add_app_callback_map_.find(id);
  DCHECK(it != add_app_callback_map_.end())
      << "Missing callback for id: " << id;
  std::move(it->second).Run(id, Error::kNone);
  add_app_callback_map_.erase(it);
}

void RemoteAppsManager::StartIconDownload(const std::string& id,
                                          const GURL& icon_url) {
  image_downloader_->Download(
      icon_url, base::BindOnce(&RemoteAppsManager::OnIconDownloaded,
                               weak_factory_.GetWeakPtr(), id));
}

void RemoteAppsManager::OnIconDownloaded(const std::string& id,
                                         const gfx::ImageSkia& icon) {
  // App may have been deleted.
  if (!model_->HasApp(id))
    return;

  RemoteAppsModel::AppInfo& app_info = model_->GetAppInfo(id);
  app_info.icon = icon;
  remote_apps_->UpdateAppIcon(id);
}

}  // namespace chromeos
