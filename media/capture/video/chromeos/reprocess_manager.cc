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

void ReprocessManager::Flush(const std::string& device_id) {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ReprocessManager::ReprocessManagerImpl::Flush,
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

void ReprocessManager::SetFpsRange(
    const std::string& device_id,
    const uint32_t stream_width,
    const uint32_t stream_height,
    const int32_t min_fps,
    const int32_t max_fps,
    cros::mojom::CrosImageCapture::SetFpsRangeCallback callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ReprocessManager::ReprocessManagerImpl::SetFpsRange,
                     base::Unretained(impl.get()), device_id, stream_width,
                     stream_height, min_fps, max_fps, std::move(callback)));
}

void ReprocessManager::GetFpsRange(const std::string& device_id,
                                   const uint32_t stream_width,
                                   const uint32_t stream_height,
                                   GetFpsRangeCallback callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ReprocessManager::ReprocessManagerImpl::GetFpsRange,
                     base::Unretained(impl.get()), device_id, stream_width,
                     stream_height, std::move(callback)));
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

void ReprocessManager::ReprocessManagerImpl::Flush(
    const std::string& device_id) {
  auto empty_queue = ReprocessTaskQueue();
  reprocess_task_queue_map_[device_id].swap(empty_queue);

  auto empty_map = ResolutionFpsRangeMap();
  resolution_fps_range_map_[device_id].swap(empty_map);
}

void ReprocessManager::ReprocessManagerImpl::GetCameraInfo(
    const std::string& device_id,
    GetCameraInfoCallback callback) {
  std::move(callback).Run(get_camera_info_.Run(device_id));
}

void ReprocessManager::ReprocessManagerImpl::SetFpsRange(
    const std::string& device_id,
    const uint32_t stream_width,
    const uint32_t stream_height,
    const int32_t min_fps,
    const int32_t max_fps,
    cros::mojom::CrosImageCapture::SetFpsRangeCallback callback) {
  const int entry_length = 2;

  auto camera_info = get_camera_info_.Run(device_id);
  auto& static_metadata = camera_info->static_camera_characteristics;
  auto available_fps_range_entries = GetMetadataEntryAsSpan<int32_t>(
      static_metadata, cros::mojom::CameraMetadataTag::
                           ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
  CHECK(available_fps_range_entries.size() % entry_length == 0);

  bool is_valid = false;
  for (size_t i = 0; i < available_fps_range_entries.size();
       i += entry_length) {
    if (available_fps_range_entries[i] == min_fps &&
        available_fps_range_entries[i + 1] == max_fps) {
      is_valid = true;
      break;
    }
  }

  auto resolution = gfx::Size(stream_width, stream_height);
  auto& fps_map = resolution_fps_range_map_[device_id];
  if (!is_valid) {
    // If the input range is invalid, we should still clear the cache range so
    // that it will fallback to use default fps range rather than the cache one.
    auto it = fps_map.find(resolution);
    if (it != fps_map.end()) {
      fps_map.erase(it);
    }
    std::move(callback).Run(false);
    return;
  }

  auto fps_range = gfx::Range(min_fps, max_fps);
  fps_map[resolution] = fps_range;
  std::move(callback).Run(true);
}

void ReprocessManager::ReprocessManagerImpl::GetFpsRange(
    const std::string& device_id,
    const uint32_t stream_width,
    const uint32_t stream_height,
    GetFpsRangeCallback callback) {
  if (resolution_fps_range_map_.find(device_id) ==
      resolution_fps_range_map_.end()) {
    std::move(callback).Run({});
    return;
  }

  auto resolution = gfx::Size(stream_width, stream_height);
  auto& fps_map = resolution_fps_range_map_[device_id];
  if (fps_map.find(resolution) == fps_map.end()) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(fps_map[resolution]);
}

bool ReprocessManager::ReprocessManagerImpl::SizeComparator::operator()(
    const gfx::Size size_1,
    const gfx::Size size_2) const {
  return size_1.width() < size_2.width() || (size_1.width() == size_2.width() &&
                                             size_1.height() < size_2.height());
}

}  // namespace media
