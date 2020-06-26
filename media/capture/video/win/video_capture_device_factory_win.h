// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactory class for Windows platforms.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <mfidl.h>
#include <windows.devices.enumeration.h>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "media/base/win/mf_initializer.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Devices::Enumeration::DeviceInformationCollection;

// Extension of VideoCaptureDeviceFactory to create and manipulate Windows
// devices, via either DirectShow or MediaFoundation APIs.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryWin
    : public VideoCaptureDeviceFactory {
 public:
  static bool PlatformSupportsMediaFoundation();

  VideoCaptureDeviceFactoryWin();
  ~VideoCaptureDeviceFactoryWin() override;

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;
  void GetCameraLocationsAsync(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback) override;

  void set_use_media_foundation_for_testing(bool use) {
    use_media_foundation_ = use;
  }

 protected:
  // Protected and virtual for testing.
  virtual bool CreateDeviceEnumMonikerDirectShow(IEnumMoniker** enum_moniker);
  virtual bool CreateDeviceFilterDirectShow(const std::string& device_id,
                                            IBaseFilter** capture_filter);
  virtual bool CreateDeviceFilterDirectShow(
      Microsoft::WRL::ComPtr<IMoniker> moniker,
      IBaseFilter** capture_filter);
  virtual bool CreateDeviceSourceMediaFoundation(const std::string& device_id,
                                                 VideoCaptureApi capture_api,
                                                 IMFMediaSource** source);
  virtual bool CreateDeviceSourceMediaFoundation(
      Microsoft::WRL::ComPtr<IMFAttributes> attributes,
      IMFMediaSource** source);
  virtual bool EnumerateDeviceSourcesMediaFoundation(
      Microsoft::WRL::ComPtr<IMFAttributes> attributes,
      IMFActivate*** devices,
      UINT32* count);
  virtual void GetSupportedFormatsDirectShow(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats);
  virtual void GetSupportedFormatsMediaFoundation(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats);

 private:
  void EnumerateDevicesUWP(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback);
  void FoundAllDevicesUWP(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback,
      IAsyncOperation<DeviceInformationCollection*>* operation);
  void DeviceInfoReady(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback);
  void GetDeviceDescriptorsMediaFoundation(
      VideoCaptureDeviceDescriptors* device_descriptors);
  void AugmentDescriptorListWithDirectShowOnlyDevices(
      VideoCaptureDeviceDescriptors* device_descriptors);
  void GetDeviceDescriptorsDirectShow(
      VideoCaptureDeviceDescriptors* device_descriptors);
  int GetNumberOfSupportedFormats(const VideoCaptureDeviceDescriptor& device);
  void GetApiSpecificSupportedFormats(
      const VideoCaptureDeviceDescriptor& device,
      VideoCaptureFormats* formats);

  bool use_media_foundation_;
  MFSessionLifetime session_;

  // For calling WinRT methods on a COM initiated thread.
  base::Thread com_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
  std::unordered_set<IAsyncOperation<DeviceInformationCollection*>*> async_ops_;
  base::WeakPtrFactory<VideoCaptureDeviceFactoryWin> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryWin);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
