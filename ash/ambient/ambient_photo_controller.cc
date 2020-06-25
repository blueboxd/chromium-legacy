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
#include "base/rand_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Topic related numbers.

// The number of requests to fetch topics.
constexpr int kNumberOfRequests = 50;

// The batch size of topics to fetch in one request.
// Magic number 2 is based on experiments that no curation on Google Photos.
constexpr int kTopicsBatchSize = 2;

// The upper bound of delay to the fetch topics. An random value will be
// generated in the range of |kTopicFetchDelayMax|/2 to |kTopicFetchDelayMax|.

// TODO(b/139953713): Change to a correct time interval.
// E.g. it will be max 36 seconds if we want to fetch 50 batches in 30 mins.
constexpr base::TimeDelta kTopicFetchDelayMax = base::TimeDelta::FromSeconds(3);

using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

void DownloadImageFromUrl(const std::string& url, DownloadCallback callback) {
  DCHECK(!url.empty());

  ImageDownloader::Get()->Download(GURL(url), NO_TRAFFIC_ANNOTATION_YET,
                                   base::BindOnce(std::move(callback)));
}

}  // namespace

AmbientPhotoController::AmbientPhotoController() {
  ambient_backedn_model_observer_.Add(&ambient_backend_model_);
}

AmbientPhotoController::~AmbientPhotoController() = default;

void AmbientPhotoController::StartScreenUpdate() {
  FetchTopics();
}

void AmbientPhotoController::StopScreenUpdate() {
  photo_refresh_timer_.Stop();
  topic_index_ = 0;
  topics_batch_fetched_ = 0;
  ambient_backend_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();
}

void AmbientPhotoController::OnTopicsChanged() {
  ++topics_batch_fetched_;
  if (topics_batch_fetched_ < kNumberOfRequests)
    ScheduleFetchTopics();

  // The first OnTopicsChanged event triggers the photo refresh.
  if (topics_batch_fetched_ == 1)
    ScheduleRefreshImage();
}

void AmbientPhotoController::FetchTopics() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchScreenUpdateInfo(
          kTopicsBatchSize,
          base::BindOnce(&AmbientPhotoController::OnScreenUpdateInfoFetched,
                         weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::ScheduleFetchTopics() {
  const base::TimeDelta kDelay =
      (base::RandDouble() * kTopicFetchDelayMax) / 2 + kTopicFetchDelayMax / 2;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AmbientPhotoController::FetchTopics,
                     weak_factory_.GetWeakPtr()),
      kDelay);
}

void AmbientPhotoController::ScheduleRefreshImage() {
  base::TimeDelta refresh_interval;
  if (!ambient_backend_model_.ShouldFetchImmediately())
    refresh_interval = kPhotoRefreshInterval;

  // |photo_refresh_timer_| will start immediately if ShouldFetchImmediately()
  // is true.
  photo_refresh_timer_.Start(
      FROM_HERE, refresh_interval,
      base::BindOnce(&AmbientPhotoController::GetNextImage,
                     weak_factory_.GetWeakPtr()));
}

const AmbientModeTopic& AmbientPhotoController::GetNextTopic() {
  const auto& topics = ambient_backend_model_.topics();
  DCHECK(!topics.empty());

  // We prefetch the first two photos, which will increase the |topic_index_| to
  // 2 in the first batch with size of 2. Then it will reset to 0 if we put this
  // block after the increment of |topic_index_|.
  if (topic_index_ == topics.size())
    topic_index_ = 0;

  return topics[topic_index_++];
}

void AmbientPhotoController::GetNextImage() {
  const AmbientModeTopic& topic = GetNextTopic();
  const std::string& image_url = topic.portrait_image_url.value_or(topic.url);
  DownloadImageFromUrl(
      image_url, base::BindOnce(&AmbientPhotoController::OnPhotoDownloaded,
                                weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::OnScreenUpdateInfoFetched(
    const ash::ScreenUpdate& screen_update) {
  // It is possible that |screen_update| is an empty instance if fatal errors
  // happened during the fetch.
  // TODO(b/148485116): Implement retry logic.
  if (screen_update.next_topics.empty() &&
      !screen_update.weather_info.has_value()) {
    LOG(ERROR) << "The screen update info fetch has failed.";
    return;
  }

  ambient_backend_model_.AppendTopics(screen_update.next_topics);
  StartDownloadingWeatherConditionIcon(screen_update);
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
