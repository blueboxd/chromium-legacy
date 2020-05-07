// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fuchsia/video_capture_device_factory_fuchsia.h"

#include <lib/sys/cpp/component_context.h>

#include "base/check_op.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/capture/video/fuchsia/video_capture_device_fuchsia.h"

namespace media {

class VideoCaptureDeviceFactoryFuchsia::DeviceInfoFetcher {
 public:
  DeviceInfoFetcher(uint64_t device_id, fuchsia::camera3::DevicePtr device)
      : device_id_(device_id), device_(std::move(device)) {
    device_.set_error_handler(
        fit::bind_member(this, &DeviceInfoFetcher::OnError));
    device_->GetIdentifier(
        fit::bind_member(this, &DeviceInfoFetcher::OnDescription));
    device_->GetConfigurations(
        fit::bind_member(this, &DeviceInfoFetcher::OnConfigurations));
  }

  ~DeviceInfoFetcher() = default;

  DeviceInfoFetcher(const DeviceInfoFetcher&) = delete;
  const DeviceInfoFetcher& operator=(const DeviceInfoFetcher&) = delete;

  void WaitResults() {
    DCHECK(!wait_results_run_loop_);

    if (have_results())
      return;

    if (!device_)
      return;

    wait_results_run_loop_.emplace();
    wait_results_run_loop_->Run();
    wait_results_run_loop_.reset();
  }

  bool have_results() const { return description_ && formats_; }
  bool is_usable() const { return device_ || have_results(); }

  uint64_t device_id() const { return device_id_; }
  const std::string& description() const { return description_.value(); }
  const VideoCaptureFormats& formats() const { return formats_.value(); }

 private:
  void OnError(zx_status_t status) {
    ZX_LOG(ERROR, status) << "fuchsia.camera3.Device disconnected";

    if (wait_results_run_loop_)
      wait_results_run_loop_->Quit();
  }

  void OnDescription(fidl::StringPtr identifier) {
    description_ = identifier.value_or("");
    MaybeSignalDone();
  }

  void OnConfigurations(std::vector<fuchsia::camera3::Configuration> configs) {
    VideoCaptureFormats formats;
    for (auto& config : configs) {
      for (auto& props : config.streams) {
        VideoCaptureFormat format;
        format.frame_size = gfx::Size(props.image_format.display_width,
                                      props.image_format.display_height);
        format.frame_rate = static_cast<float>(props.frame_rate.numerator) /
                            props.frame_rate.denominator;
        format.pixel_format =
            VideoCaptureDeviceFuchsia::GetConvertedPixelFormat(
                props.image_format.pixel_format.type);
        if (format.pixel_format == PIXEL_FORMAT_UNKNOWN)
          continue;
        formats.push_back(format);
      }
    }

    formats_ = std::move(formats);
    MaybeSignalDone();
  }

  void MaybeSignalDone() {
    if (!have_results())
      return;

    device_.Unbind();

    if (wait_results_run_loop_)
      wait_results_run_loop_->Quit();
  }

  uint64_t device_id_;
  fuchsia::camera3::DevicePtr device_;
  base::Optional<std::string> description_;
  base::Optional<VideoCaptureFormats> formats_;
  base::Optional<base::RunLoop> wait_results_run_loop_;
};

VideoCaptureDeviceFactoryFuchsia::VideoCaptureDeviceFactoryFuchsia() {
  DETACH_FROM_THREAD(thread_checker_);
}

VideoCaptureDeviceFactoryFuchsia::~VideoCaptureDeviceFactoryFuchsia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::unique_ptr<VideoCaptureDevice>
VideoCaptureDeviceFactoryFuchsia::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint64_t device_id;
  bool converted =
      base::StringToUint64(device_descriptor.device_id, &device_id);

  // Test may call CreateDevice() with an invalid |device_id|.
  if (!converted)
    return nullptr;

  fidl::InterfaceHandle<fuchsia::camera3::Device> device;
  device_watcher_->ConnectToDevice(device_id, device.NewRequest());
  return std::make_unique<VideoCaptureDeviceFuchsia>(std::move(device));
}

void VideoCaptureDeviceFactoryFuchsia::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  device_descriptors->clear();

  if (!device_watcher_) {
    DCHECK(!first_update_run_loop_);
    DCHECK(devices_.empty());

    Initialize();

    // The RunLoop will quit when either we've received the first WatchDevices()
    // response or DeviceWatcher fails. |devices_| will be empty in case of a
    // failure.
    first_update_run_loop_.emplace();
    first_update_run_loop_->Run();
    first_update_run_loop_.reset();
  }

  for (auto& d : devices_) {
    d.second->WaitResults();
    if (d.second->is_usable()) {
      device_descriptors->push_back(VideoCaptureDeviceDescriptor(
          d.second->description(), base::NumberToString(d.first),
          VideoCaptureApi::FUCHSIA_CAMERA3));
    }
  }
}

void VideoCaptureDeviceFactoryFuchsia::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device,
    VideoCaptureFormats* capture_formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint64_t device_id;
  bool converted = base::StringToUint64(device.device_id, &device_id);
  DCHECK(converted);

  auto it = devices_.find(device_id);
  if (it == devices_.end()) {
    capture_formats->clear();
  } else {
    it->second->WaitResults();
    *capture_formats = it->second->formats();
  }
}

void VideoCaptureDeviceFactoryFuchsia::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!device_watcher_);
  DCHECK(devices_.empty());

  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      device_watcher_.NewRequest());

  device_watcher_.set_error_handler(fit::bind_member(
      this, &VideoCaptureDeviceFactoryFuchsia::OnDeviceWatcherDisconnected));

  WatchDevices();
}

void VideoCaptureDeviceFactoryFuchsia::OnDeviceWatcherDisconnected(
    zx_status_t status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ZX_LOG(ERROR, status) << "fuchsia.camera3.DeviceWatcher disconnected.";
  devices_.clear();

  if (first_update_run_loop_)
    first_update_run_loop_->Quit();
}

void VideoCaptureDeviceFactoryFuchsia::WatchDevices() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  device_watcher_->WatchDevices(fit::bind_member(
      this, &VideoCaptureDeviceFactoryFuchsia::OnWatchDevicesResult));
}

void VideoCaptureDeviceFactoryFuchsia::OnWatchDevicesResult(
    std::vector<fuchsia::camera3::WatchDevicesEvent> events) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto& e : events) {
    if (e.is_removed()) {
      int erased = devices_.erase(e.removed());
      if (!erased) {
        LOG(WARNING) << "Received device removed event for a device that "
                        "wasn't previously registered.";
      }
      continue;
    }

    uint64_t id;
    if (e.is_added()) {
      id = e.added();
      if (devices_.find(id) != devices_.end()) {
        LOG(WARNING) << "Received device added event for a device that was "
                        "previously registered.";
        continue;
      }
    } else {
      id = e.existing();
      if (devices_.find(id) != devices_.end()) {
        continue;
      }
      LOG(WARNING) << "Received device exists event for a device that wasn't "
                      "previously registered.";
    }

    fuchsia::camera3::DevicePtr device;
    device_watcher_->ConnectToDevice(id, device.NewRequest());
    devices_.emplace(
        id, std::make_unique<DeviceInfoFetcher>(id, std::move(device)));
  }

  if (first_update_run_loop_)
    first_update_run_loop_->Quit();

  // Watch for further updates.
  WatchDevices();
}

}  // namespace media
