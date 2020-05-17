// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <string>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

void DownloadImageFromUrl(const std::string& url, DownloadCallback callback) {
  DCHECK(!url.empty());

  ImageDownloader::Get()->Download(GURL(url), NO_TRAFFIC_ANNOTATION_YET,
                                   base::BindOnce(std::move(callback)));
}

}  // namespace

AmbientPhotoController::AmbientPhotoController() = default;

AmbientPhotoController::~AmbientPhotoController() = default;

void AmbientPhotoController::StartScreenUpdate() {
  RefreshImage();
}

void AmbientPhotoController::StopScreenUpdate() {
  photo_refresh_timer_.Stop();
  ambient_backend_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();
}

void AmbientPhotoController::RefreshImage() {
  if (ambient_backend_model_.ShouldFetchImmediately()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AmbientPhotoController::GetNextScreenUpdateInfo,
                       weak_factory_.GetWeakPtr()));
  } else {
    ambient_backend_model_.ShowNextImage();
    ScheduleRefreshImage();
  }
}

void AmbientPhotoController::ScheduleRefreshImage() {
  base::TimeDelta refresh_interval;
  if (!ambient_backend_model_.ShouldFetchImmediately())
    refresh_interval = kPhotoRefreshInterval;

  // |photo_refresh_timer_| will start immediately if ShouldFetchImmediately()
  // is true.
  // TODO(b/156271483): Consolidate RefreshImage() and ScheduleRefreshImage() to
  // only check ShouldFetchImmediately() once.
  photo_refresh_timer_.Start(
      FROM_HERE, refresh_interval,
      base::BindOnce(&AmbientPhotoController::RefreshImage,
                     weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::GetNextScreenUpdateInfo() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchScreenUpdateInfo(
          base::BindOnce(&AmbientPhotoController::OnNextScreenUpdateInfoFetched,
                         weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::OnNextScreenUpdateInfoFetched(
    const ash::ScreenUpdate& screen_update) {
  // It is possible that |screen_update| is an empty instance if fatal errors
  // happened during the fetch.
  // TODO(b/148485116): Implement retry logic.
  if (screen_update.next_topics.empty() &&
      !screen_update.weather_info.has_value()) {
    LOG(ERROR) << "The screen update info fetch has failed.";
    return;
  }

  StartDownloadingPhotoImage(screen_update);
  StartDownloadingWeatherConditionIcon(screen_update);
}

void AmbientPhotoController::StartDownloadingPhotoImage(
    const ash::ScreenUpdate& screen_update) {
  // We specified the size of |next_topics| in the request. So if nothing goes
  // wrong, we should get that amount of |Topic| in the response.
  if (screen_update.next_topics.size() == 0) {
    LOG(ERROR) << "No topics included in the response.";
    OnPhotoDownloaded(gfx::ImageSkia());
    return;
  }

  // TODO(b/148462257): Handle a batch of topics.
  ash::AmbientModeTopic topic = screen_update.next_topics[0];
  const std::string& image_url = topic.portrait_image_url.value_or(topic.url);
  DownloadImageFromUrl(
      image_url, base::BindOnce(&AmbientPhotoController::OnPhotoDownloaded,
                                weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::StartDownloadingWeatherConditionIcon(
    const ash::ScreenUpdate& screen_update) {
  if (!screen_update.weather_info.has_value()) {
    LOG(WARNING) << "No weather info included in the response.";
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

  DownloadImageFromUrl(
      icon_url,
      base::BindOnce(&AmbientPhotoController::OnWeatherConditionIconDownloaded,
                     weak_factory_.GetWeakPtr(),
                     screen_update.weather_info->temp_f));
}

void AmbientPhotoController::OnPhotoDownloaded(const gfx::ImageSkia& image) {
  if (!image.isNull())
    ambient_backend_model_.AddNextImage(image);

  ScheduleRefreshImage();
}

void AmbientPhotoController::OnWeatherConditionIconDownloaded(
    base::Optional<float> temp_f,
    const gfx::ImageSkia& icon) {
  // For now we only show the weather card when both fields have values.
  // TODO(meilinw): optimize the behavior with more specific error handling.
  if (icon.isNull() || !temp_f.has_value())
    return;

  ambient_backend_model_.UpdateWeatherInfo(icon, temp_f.value());
}

}  // namespace ash
