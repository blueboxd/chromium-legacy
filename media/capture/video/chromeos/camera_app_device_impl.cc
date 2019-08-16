// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_impl.h"

#include "media/capture/video/chromeos/camera_metadata_utils.h"

namespace media {

namespace {

void OnStillCaptureDone(media::mojom::ImageCapture::TakePhotoCallback callback,
                        int status,
                        mojom::BlobPtr blob) {
  DCHECK_EQ(status, kReprocessSuccess);
  std::move(callback).Run(std::move(blob));
}

}  // namespace

ReprocessTask::ReprocessTask() = default;

ReprocessTask::ReprocessTask(ReprocessTask&& other)
    : effect(other.effect),
      callback(std::move(other.callback)),
      extra_metadata(std::move(other.extra_metadata)) {}

ReprocessTask::~ReprocessTask() = default;

bool CameraAppDeviceImpl::SizeComparator::operator()(
    const gfx::Size& size_1,
    const gfx::Size& size_2) const {
  return size_1.width() < size_2.width() || (size_1.width() == size_2.width() &&
                                             size_1.height() < size_2.height());
}

// static
int CameraAppDeviceImpl::GetReprocessReturnCode(
    cros::mojom::Effect effect,
    const cros::mojom::CameraMetadataPtr* metadata) {
  if (effect == cros::mojom::Effect::PORTRAIT_MODE) {
    auto portrait_mode_segmentation_result = GetMetadataEntryAsSpan<uint8_t>(
        *metadata, static_cast<cros::mojom::CameraMetadataTag>(
                       kPortraitModeSegmentationResultVendorKey));
    DCHECK(!portrait_mode_segmentation_result.empty());
    return static_cast<int>(portrait_mode_segmentation_result[0]);
  }
  return kReprocessSuccess;
}

// static
ReprocessTaskQueue CameraAppDeviceImpl::GetSingleShotReprocessOptions(
    media::mojom::ImageCapture::TakePhotoCallback take_photo_callback) {
  ReprocessTaskQueue result_task_queue;
  ReprocessTask still_capture_task;
  still_capture_task.effect = cros::mojom::Effect::NO_EFFECT;
  still_capture_task.callback =
      base::BindOnce(&OnStillCaptureDone, std::move(take_photo_callback));
  result_task_queue.push(std::move(still_capture_task));
  return result_task_queue;
}

CameraAppDeviceImpl::CameraAppDeviceImpl(const std::string& device_id,
                                         cros::mojom::CameraInfoPtr camera_info)
    : device_id_(device_id),
      camera_info_(std::move(camera_info)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(
          std::make_unique<base::WeakPtrFactory<CameraAppDeviceImpl>>(this)) {}

CameraAppDeviceImpl::~CameraAppDeviceImpl() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(weak_ptr_factory_));
}

void CameraAppDeviceImpl::BindReceiver(
    mojo::PendingReceiver<cros::mojom::CameraAppDevice> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CameraAppDeviceImpl::ConsumeReprocessOptions(
    media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
    base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback) {
  ReprocessTaskQueue result_task_queue;

  ReprocessTask still_capture_task;
  still_capture_task.effect = cros::mojom::Effect::NO_EFFECT;
  still_capture_task.callback =
      base::BindOnce(&OnStillCaptureDone, std::move(take_photo_callback));
  result_task_queue.push(std::move(still_capture_task));

  base::AutoLock lock(reprocess_tasks_lock_);

  while (!reprocess_task_queue_.empty()) {
    result_task_queue.push(std::move(reprocess_task_queue_.front()));
    reprocess_task_queue_.pop();
  }
  std::move(consumption_callback).Run(std::move(result_task_queue));
}

void CameraAppDeviceImpl::GetFpsRange(const gfx::Size& resolution,
                                      GetFpsRangeCallback callback) {
  base::AutoLock lock(fps_ranges_lock_);

  auto it = resolution_fps_range_map_.find(resolution);
  if (it == resolution_fps_range_map_.end()) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(it->second);
}

void CameraAppDeviceImpl::SetReprocessResult(
    SetReprocessOptionCallback callback,
    const int32_t status,
    media::mojom::BlobPtr blob) {
  auto callback_on_mojo_thread = base::BindOnce(
      [](const int32_t status, media::mojom::BlobPtr blob,
         SetReprocessOptionCallback callback) {
        std::move(callback).Run(status, std::move(blob));
      },
      status, std::move(blob), std::move(callback));
  task_runner_->PostTask(FROM_HERE, std::move(callback_on_mojo_thread));
}

void CameraAppDeviceImpl::GetCameraInfo(GetCameraInfoCallback callback) {
  DCHECK(camera_info_);
  std::move(callback).Run(camera_info_.Clone());
}

void CameraAppDeviceImpl::SetReprocessOption(
    cros::mojom::Effect effect,
    SetReprocessOptionCallback reprocess_result_callback) {
  ReprocessTask task;
  task.effect = effect;
  task.callback = base::BindOnce(&CameraAppDeviceImpl::SetReprocessResult,
                                 weak_ptr_factory_->GetWeakPtr(),
                                 std::move(reprocess_result_callback));

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

  base::AutoLock lock(reprocess_tasks_lock_);

  reprocess_task_queue_.push(std::move(task));
}

void CameraAppDeviceImpl::SetFpsRange(const gfx::Size& resolution,
                                      const gfx::Range& fps_range,
                                      SetFpsRangeCallback callback) {
  const int entry_length = 2;

  auto& static_metadata = camera_info_->static_camera_characteristics;
  auto available_fps_range_entries = GetMetadataEntryAsSpan<int32_t>(
      static_metadata, cros::mojom::CameraMetadataTag::
                           ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
  DCHECK(available_fps_range_entries.size() % entry_length == 0);

  bool is_valid = false;
  int min_fps = static_cast<int>(fps_range.GetMin());
  int max_fps = static_cast<int>(fps_range.GetMax());
  for (size_t i = 0; i < available_fps_range_entries.size();
       i += entry_length) {
    if (available_fps_range_entries[i] == min_fps &&
        available_fps_range_entries[i + 1] == max_fps) {
      is_valid = true;
      break;
    }
  }

  base::AutoLock lock(fps_ranges_lock_);

  if (!is_valid) {
    // If the input range is invalid, we should still clear the cache range so
    // that it will fallback to use default fps range rather than the cache one.
    auto it = resolution_fps_range_map_.find(resolution);
    if (it != resolution_fps_range_map_.end()) {
      resolution_fps_range_map_.erase(it);
    }
    std::move(callback).Run(false);
    return;
  }

  resolution_fps_range_map_[resolution] = fps_range;
  std::move(callback).Run(true);
}

}  // namespace media
