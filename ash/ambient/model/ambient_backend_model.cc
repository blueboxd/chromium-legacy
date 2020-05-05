// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_backend_model.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"

namespace ash {

AmbientBackendModel::AmbientBackendModel() {
  SetPhotoRefreshInterval(kPhotoRefreshInterval);
}

AmbientBackendModel::~AmbientBackendModel() = default;

void AmbientBackendModel::AddObserver(AmbientBackendModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AmbientBackendModel::RemoveObserver(
    AmbientBackendModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AmbientBackendModel::ShouldFetchImmediately() const {
  // If currently shown image is close to the end of images cache, we prefetch
  // more image.
  const int next_load_image_index = GetImageBufferLength() / 2;
  return images_.empty() ||
         current_image_index_ >
             static_cast<int>(images_.size() - 1 - next_load_image_index);
}

void AmbientBackendModel::ShowNextImage() {
  // Do not show next if have not downloaded enough images.
  if (ShouldFetchImmediately())
    return;

  const int max_current_image_index = GetImageBufferLength() / 2;
  if (current_image_index_ >= max_current_image_index) {
    // Pop the first image and keep |current_image_index_| unchanged, will be
    // equivalent to show next image.
    images_.pop_front();
  } else {
    ++current_image_index_;
  }
  NotifyImagesChanged();
}

void AmbientBackendModel::AddNextImage(const gfx::ImageSkia& image) {
  images_.emplace_back(image);

  // Update the first two images. The second image is used for photo transition
  // animation.
  if (images_.size() <= 2)
    NotifyImagesChanged();
}

base::TimeDelta AmbientBackendModel::GetPhotoRefreshInterval() {
  if (ShouldFetchImmediately())
    return base::TimeDelta();

  return photo_refresh_interval_;
}

void AmbientBackendModel::SetPhotoRefreshInterval(base::TimeDelta interval) {
  photo_refresh_interval_ = interval;
}

void AmbientBackendModel::Clear() {
  images_.clear();
  current_image_index_ = 0;
}

gfx::ImageSkia AmbientBackendModel::GetPrevImage() const {
  if (current_image_index_ == 0)
    return gfx::ImageSkia();

  return images_[current_image_index_ - 1];
}

gfx::ImageSkia AmbientBackendModel::GetCurrImage() const {
  if (images_.empty())
    return gfx::ImageSkia();

  return images_[current_image_index_];
}

gfx::ImageSkia AmbientBackendModel::GetNextImage() const {
  if (images_.empty() ||
      static_cast<int>(images_.size() - current_image_index_) == 1) {
    return gfx::ImageSkia();
  }

  return images_[current_image_index_ + 1];
}

void AmbientBackendModel::UpdateWeatherInfo(
    const gfx::ImageSkia& weather_condition_icon,
    float temperature) {
  weather_condition_icon_ = weather_condition_icon;
  temperature_ = temperature;

  if (!weather_condition_icon.isNull())
    NotifyWeatherInfoUpdated();
}

void AmbientBackendModel::NotifyImagesChanged() {
  for (auto& observer : observers_)
    observer.OnImagesChanged();
}

int AmbientBackendModel::GetImageBufferLength() const {
  return buffer_length_for_testing_ == -1 ? kImageBufferLength
                                          : buffer_length_for_testing_;
}

void AmbientBackendModel::NotifyWeatherInfoUpdated() {
  for (auto& observer : observers_)
    observer.OnWeatherInfoUpdated();
}

}  // namespace ash
