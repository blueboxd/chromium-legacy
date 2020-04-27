// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/photo_controller_impl.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"

namespace {
using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

void DownloadImageFromUrl(const std::string& url, DownloadCallback callback) {
  DCHECK(!url.empty());
  AccountId account_id =
      chromeos::ProfileHelper::Get()
          ->GetUserByProfile(ProfileManager::GetActiveUserProfile())
          ->GetAccountId();
  ash::AssistantImageDownloader::GetInstance()->Download(account_id, GURL(url),
                                                         std::move(callback));
}

}  // namespace

PhotoControllerImpl::PhotoControllerImpl()
    : photo_client_(PhotoClient::Create()) {}

PhotoControllerImpl::~PhotoControllerImpl() = default;

void PhotoControllerImpl::GetNextScreenUpdateInfo(
    PhotoDownloadCallback photo_callback,
    WeatherIconDownloadCallback icon_callback) {
  photo_client_->FetchScreenUpdateInfo(
      base::BindOnce(&PhotoControllerImpl::OnNextScreenUpdateInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(photo_callback),
                     std::move(icon_callback)));
}

void PhotoControllerImpl::GetSettings(GetSettingsCallback callback) {
  photo_client_->GetSettings(std::move(callback));
}

void PhotoControllerImpl::UpdateSettings(int topic_source,
                                         UpdateSettingsCallback callback) {
  photo_client_->UpdateSettings(topic_source, std::move(callback));
}

void PhotoControllerImpl::OnNextScreenUpdateInfoFetched(
    PhotoDownloadCallback photo_callback,
    WeatherIconDownloadCallback icon_callback,
    const ash::PhotoController::ScreenUpdate& screen_update) {
  // It is possible that |screen_update| is an empty instance if fatal errors
  // happened during the fetch.
  if (screen_update.next_topics.size() == 0 &&
      !screen_update.weather_info.has_value()) {
    LOG(ERROR) << "The screen update info fetch has failed.";
    std::move(photo_callback).Run(gfx::ImageSkia());
    std::move(icon_callback).Run(base::nullopt, gfx::ImageSkia());
    return;
  }

  StartDownloadingPhotoImage(screen_update, std::move(photo_callback));
  StartDownloadingWeatherConditionIcon(screen_update, std::move(icon_callback));
}

void PhotoControllerImpl::StartDownloadingPhotoImage(
    const ash::PhotoController::ScreenUpdate& screen_update,
    PhotoDownloadCallback photo_callback) {
  // We specified the size of |next_topics| in the request. So if nothing goes
  // wrong, we should get that amount of |Topic| in the response.
  if (screen_update.next_topics.size() == 0) {
    LOG(ERROR) << "No topics included in the response.";
    std::move(photo_callback).Run(gfx::ImageSkia());
    return;
  }

  // TODO(b/148462257): Handle a batch of topics.
  ash::PhotoController::Topic topic = screen_update.next_topics[0];
  const std::string& image_url = topic.portrait_image_url.value_or(topic.url);
  DownloadImageFromUrl(image_url, base::BindOnce(std::move(photo_callback)));
}

void PhotoControllerImpl::StartDownloadingWeatherConditionIcon(
    const ash::PhotoController::ScreenUpdate& screen_update,
    WeatherIconDownloadCallback icon_callback) {
  if (!screen_update.weather_info.has_value()) {
    LOG(WARNING) << "No weather info included in the response.";
    std::move(icon_callback).Run(base::nullopt, gfx::ImageSkia());
    return;
  }

  // Ideally we should avoid downloading from the same url again to reduce the
  // overhead, as it's unlikely that the weather condition is changing
  // frequently during the day.
  // TODO(meilinw): avoid repeated downloading by caching the last N url hashes,
  // where N should depend on the icon image size.
  const std::string& icon_url =
      screen_update.weather_info->condition_icon_url.value_or(std::string());
  if (icon_url.empty()) {
    LOG(ERROR) << "No value found for condition icon url in the weather info "
                  "response.";
    return;
  }

  DownloadImageFromUrl(icon_url,
                       base::BindOnce(std::move(icon_callback),
                                      screen_update.weather_info->temp_f));
}
