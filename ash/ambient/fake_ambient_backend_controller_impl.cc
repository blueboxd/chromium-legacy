// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/fake_ambient_backend_controller_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace ash {

namespace {

constexpr AmbientModeTopicSource kTopicSource =
    AmbientModeTopicSource::kGooglePhotos;

constexpr char kFakeUrl[] = "chrome://ambient";

}  // namespace

FakeAmbientBackendControllerImpl::FakeAmbientBackendControllerImpl() = default;
FakeAmbientBackendControllerImpl::~FakeAmbientBackendControllerImpl() = default;

void FakeAmbientBackendControllerImpl::FetchScreenUpdateInfo(
    int num_topics,
    OnScreenUpdateInfoFetchedCallback callback) {
  ash::AmbientModeTopic topic;
  topic.url = kFakeUrl;

  ash::WeatherInfo weather_info;
  weather_info.temp_f = .0f;
  weather_info.condition_icon_url = kFakeUrl;

  ash::ScreenUpdate update;
  update.next_topics.emplace_back(topic);
  update.weather_info = weather_info;

  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), update));
}

void FakeAmbientBackendControllerImpl::GetSettings(
    GetSettingsCallback callback) {
  AmbientSettings settings;
  settings.topic_source = kTopicSource;
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), settings));
}

void FakeAmbientBackendControllerImpl::UpdateSettings(
    const AmbientSettings& settings,
    UpdateSettingsCallback callback) {
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
}

void FakeAmbientBackendControllerImpl::FetchPersonalAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    const std::string& resume_token,
    OnPersonalAlbumsFetchedCallback callback) {
  PersonalAlbums albums;
  PersonalAlbum album;
  album.album_id = "0";
  album.album_name = "test";
  albums.albums.emplace_back(std::move(album));
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(albums)));
}

void FakeAmbientBackendControllerImpl::SetPhotoRefreshInterval(
    base::TimeDelta interval) {
  NOTIMPLEMENTED();
}

}  // namespace ash
