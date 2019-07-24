// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_

#include <map>
#include <utility>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace video_capture {

// Decorator that adds support for virtual devices to a given DeviceFactory.
class VirtualDeviceEnabledDeviceFactory : public DeviceFactory {
 public:
  explicit VirtualDeviceEnabledDeviceFactory(
      std::unique_ptr<DeviceFactory> factory);
  ~VirtualDeviceEnabledDeviceFactory() override;

  // DeviceFactory implementation.
  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(const std::string& device_id,
                    mojom::DeviceRequest device_request,
                    CreateDeviceCallback callback) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::ProducerPtr producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors,
      mojom::SharedMemoryVirtualDeviceRequest virtual_device) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::TextureVirtualDeviceRequest virtual_device) override;
  void RegisterVirtualDevicesChangedObserver(
      mojom::DevicesChangedObserverPtr observer,
      bool raise_event_if_virtual_devices_already_present) override;

#if defined(OS_CHROMEOS)
  void BindCrosImageCaptureRequest(
      cros::mojom::CrosImageCaptureRequest request) override;
#endif  // defined(OS_CHROMEOS)

 private:
  class VirtualDeviceEntry;

  void OnGetDeviceInfos(
      GetDeviceInfosCallback callback,
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  void OnVirtualDeviceProducerConnectionErrorOrClose(
      const std::string& device_id);
  void OnVirtualDeviceConsumerConnectionErrorOrClose(
      const std::string& device_id);
  void EmitDevicesChangedEvent();
  void OnDevicesChangedObserverDisconnected(
      mojom::DevicesChangedObserverPtr::Proxy* observer);

  std::map<std::string, VirtualDeviceEntry> virtual_devices_by_id_;
  const std::unique_ptr<DeviceFactory> device_factory_;
  std::vector<mojom::DevicesChangedObserverPtr> devices_changed_observers_;

  base::WeakPtrFactory<VirtualDeviceEnabledDeviceFactory> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(VirtualDeviceEnabledDeviceFactory);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
