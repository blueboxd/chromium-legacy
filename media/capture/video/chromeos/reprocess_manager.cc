// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/reprocess_manager.h"

#include <functional>
#include <utility>

#include "media/capture/video/chromeos/camera_metadata_utils.h"

namespace media {

namespace {

void OnStillCaptureDone(media::mojom::ImageCapture::TakePhotoCallback callback,
                        int status,
                        mojom::BlobPtr blob) {
  std::move(callback).Run(std::move(blob));
}

}  // namespace

ReprocessTask::ReprocessTask() = default;

ReprocessTask::ReprocessTask(ReprocessTask&& other)
    : effect(other.effect),
      callback(std::move(other.callback)),
      extra_metadata(std::move(other.extra_metadata)) {}

ReprocessTask::~ReprocessTask() = default;

// static
int ReprocessManager::GetReprocessReturnCode(
    cros::mojom::Effect effect,
    const cros::mojom::CameraMetadataPtr* metadata) {
  if (effect == cros::mojom::Effect::PORTRAIT_MODE) {
    auto* portrait_mode_segmentation_result = GetMetadataEntry(
        *metadata, static_cast<cros::mojom::CameraMetadataTag>(
                       kPortraitModeSegmentationResultVendorKey));
    CHECK(portrait_mode_segmentation_result);
    return static_cast<int>((*portrait_mode_segmentation_result)->data[0]);
  }
  return kReprocessSuccess;
}

ReprocessManager::ReprocessManager(CameraInfoGetter get_camera_info)
    : sequenced_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::USER_VISIBLE})),
      impl(std::make_unique<ReprocessManager::ReprocessManagerImpl>(
          std::move(get_camera_info))) {}

ReprocessManager::~ReprocessManager() {
  sequenced_task_runner_->DeleteSoon(FROM_HERE, std::move(impl));
}

void ReprocessManager::SetReprocessOption(
    const std::string& device_id,
    cros::mojom::Effect effect,
    cros::mojom::CrosImageCapture::SetReprocessOptionCallback
        reprocess_result_callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReprocessManager::ReprocessManagerImpl::SetReprocessOption,
          base::Unretained(impl.get()), device_id, effect,
          std::move(reprocess_result_callback)));
}

void ReprocessManager::ConsumeReprocessOptions(
    const std::string& device_id,
    media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
    base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReprocessManager::ReprocessManagerImpl::ConsumeReprocessOptions,
          base::Unretained(impl.get()), device_id,
          std::move(take_photo_callback), std::move(consumption_callback)));
}

void ReprocessManager::FlushReprocessOptions(const std::string& device_id) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReprocessManager::ReprocessManagerImpl::FlushReprocessOptions,
          base::Unretained(impl.get()), device_id));
}

void ReprocessManager::GetCameraInfo(const std::string& device_id,
                                     GetCameraInfoCallback callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ReprocessManager::ReprocessManagerImpl::GetCameraInfo,
                     base::Unretained(impl.get()), device_id,
                     std::move(callback)));
}

ReprocessManager::ReprocessManagerImpl::ReprocessManagerImpl(
    CameraInfoGetter get_camera_info)
    : get_camera_info_(std::move(get_camera_info)) {}

ReprocessManager::ReprocessManagerImpl::~ReprocessManagerImpl() = default;

void ReprocessManager::ReprocessManagerImpl::SetReprocessOption(
    const std::string& device_id,
    cros::mojom::Effect effect,
    cros::mojom::CrosImageCapture::SetReprocessOptionCallback
        reprocess_result_callback) {
  ReprocessTask task;
  task.effect = effect;
  task.callback = std::move(reprocess_result_callback);

  if (effect == cros::mojom::Effect::PORTRAIT_MODE) {
    std::vector<uint8_t> portrait_mode_value(sizeof(int32_t));
    *reinterpret_cast<int32_t*>(portrait_mode_value.data()) = 1;
    cros::mojom::CameraMetadataEntryPtr e =
        cros::mojom::CameraMetadataEntry::New();
    e->tag =
        static_cast<cros::mojom::CameraMetadataTag>(kPortraitModeVendorKey);
    e->type = cros::mojom::EntryType::TYPE_INT32;
    e->count = 1;
    e->data = std::move(portrait_mode_value);
    task.extra_metadata.push_back(std::move(e));
  }

  reprocess_task_queue_map_[device_id].push(std::move(task));
}

void ReprocessManager::ReprocessManagerImpl::ConsumeReprocessOptions(
    const std::string& device_id,
    media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
    base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback) {
  ReprocessTaskQueue result_task_queue;

  ReprocessTask still_capture_task;
  still_capture_task.effect = cros::mojom::Effect::NO_EFFECT;
  still_capture_task.callback =
      base::BindOnce(&OnStillCaptureDone, std::move(take_photo_callback));
  result_task_queue.push(std::move(still_capture_task));

  auto& task_queue = reprocess_task_queue_map_[device_id];
  while (!task_queue.empty()) {
    result_task_queue.push(std::move(task_queue.front()));
    task_queue.pop();
  }
  std::move(consumption_callback).Run(std::move(result_task_queue));
}

void ReprocessManager::ReprocessManagerImpl::FlushReprocessOptions(
    const std::string& device_id) {
  auto empty_queue = ReprocessTaskQueue();
  reprocess_task_queue_map_[device_id].swap(empty_queue);
}

void ReprocessManager::ReprocessManagerImpl::GetCameraInfo(
    const std::string& device_id,
    GetCameraInfoCallback callback) {
  std::move(callback).Run(get_camera_info_.Run(device_id));
}

}  // namespace media
