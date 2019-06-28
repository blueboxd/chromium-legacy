// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_FACTORY_CHROMEOS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_FACTORY_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

using MojoMjpegDecodeAcceleratorFactoryCB = base::RepeatingCallback<void(
    chromeos_camera::mojom::MjpegDecodeAcceleratorRequest)>;

class ReprocessManager;

class CAPTURE_EXPORT VideoCaptureDeviceFactoryChromeOS final
    : public VideoCaptureDeviceFactory {
 public:
  explicit VideoCaptureDeviceFactoryChromeOS(
      scoped_refptr<base::SingleThreadTaskRunner>
          task_runner_for_screen_observer);

  ~VideoCaptureDeviceFactoryChromeOS() override;

  // VideoCaptureDeviceFactory interface implementations.
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) final;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) final;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) final;

  static gpu::GpuMemoryBufferManager* GetBufferManager();
  static void SetGpuBufferManager(gpu::GpuMemoryBufferManager* buffer_manager);

  void BindCrosImageCaptureRequest(
      cros::mojom::CrosImageCaptureRequest request);

 private:
  // Initializes the factory. The factory is functional only after this call
  // succeeds.
  bool Init();

  // Gets camera info for the given |device_id|. Returns null CameraInfoPtr on
  // error.
  cros::mojom::CameraInfoPtr GetCameraInfo(const std::string& device_id);

  const scoped_refptr<base::SingleThreadTaskRunner>
      task_runner_for_screen_observer_;

  // The thread that all the Mojo operations of |camera_hal_delegate_| take
  // place.  Started in Init and stopped when the class instance is destroyed.
  base::Thread camera_hal_ipc_thread_;

  // Communication interface to the camera HAL.  |camera_hal_delegate_| is
  // created on the thread on which Init is called.  All the Mojo communication
  // that |camera_hal_delegate_| issues and receives must be sequenced through
  // |camera_hal_ipc_thread_|.
  scoped_refptr<CameraHalDelegate> camera_hal_delegate_;

  std::unique_ptr<ReprocessManager> reprocess_manager_;

  bool initialized_;

  base::WeakPtrFactory<VideoCaptureDeviceFactoryChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryChromeOS);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_FACTORY_CHROMEOS_H_
