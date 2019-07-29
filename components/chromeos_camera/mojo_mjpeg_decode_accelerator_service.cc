// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator_service.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace {

bool VerifyDecodeParams(const gfx::Size& coded_size,
                        mojo::ScopedSharedBufferHandle* output_handle,
                        uint32_t output_buffer_size) {
  const int kJpegMaxDimension = UINT16_MAX;
  if (coded_size.IsEmpty() || coded_size.width() > kJpegMaxDimension ||
      coded_size.height() > kJpegMaxDimension) {
    LOG(ERROR) << "invalid coded_size " << coded_size.ToString();
    return false;
  }

  if (!output_handle->is_valid()) {
    LOG(ERROR) << "invalid output_handle";
    return false;
  }

  uint32_t allocation_size =
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, coded_size);
  if (output_buffer_size < allocation_size) {
    DLOG(ERROR) << "output_buffer_size is too small: " << output_buffer_size
                << ". It needs: " << allocation_size;
    return false;
  }

  return true;
}

}  // namespace

namespace chromeos_camera {

// static
void MojoMjpegDecodeAcceleratorService::Create(
    chromeos_camera::mojom::MjpegDecodeAcceleratorRequest request) {
  auto* jpeg_decoder = new MojoMjpegDecodeAcceleratorService();
  mojo::MakeStrongBinding(base::WrapUnique(jpeg_decoder), std::move(request));
}

MojoMjpegDecodeAcceleratorService::MojoMjpegDecodeAcceleratorService()
    : accelerator_factory_functions_(
          GpuMjpegDecodeAcceleratorFactory::GetAcceleratorFactories()) {}

MojoMjpegDecodeAcceleratorService::~MojoMjpegDecodeAcceleratorService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void MojoMjpegDecodeAcceleratorService::VideoFrameReady(
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyDecodeStatus(
      bitstream_buffer_id,
      ::chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS);
}

void MojoMjpegDecodeAcceleratorService::NotifyError(
    int32_t bitstream_buffer_id,
    ::chromeos_camera::MjpegDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyDecodeStatus(bitstream_buffer_id, error);
}

void MojoMjpegDecodeAcceleratorService::Initialize(
    InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When adding non-chromeos platforms, VideoCaptureGpuJpegDecoder::Initialize
  // needs to be updated.

  std::unique_ptr<::chromeos_camera::MjpegDecodeAccelerator> accelerator;
  for (const auto& create_jda_function : accelerator_factory_functions_) {
    std::unique_ptr<::chromeos_camera::MjpegDecodeAccelerator> tmp_accelerator =
        create_jda_function.Run(base::ThreadTaskRunnerHandle::Get());
    if (tmp_accelerator && tmp_accelerator->Initialize(this)) {
      accelerator = std::move(tmp_accelerator);
      break;
    }
  }

  if (!accelerator) {
    DLOG(ERROR) << "JPEG accelerator initialization failed";
    std::move(callback).Run(false);
    return;
  }

  accelerator_ = std::move(accelerator);
  std::move(callback).Run(true);
}

void MojoMjpegDecodeAcceleratorService::Decode(
    media::BitstreamBuffer input_buffer,
    const gfx::Size& coded_size,
    mojo::ScopedSharedBufferHandle output_handle,
    uint32_t output_buffer_size,
    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("jpeg", "MojoMjpegDecodeAcceleratorService::Decode");

  DCHECK_EQ(decode_cb_map_.count(input_buffer.id()), 0u);
  if (decode_cb_map_.count(input_buffer.id()) != 0) {
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  decode_cb_map_[input_buffer.id()] = std::move(callback);

  if (!VerifyDecodeParams(coded_size, &output_handle, output_buffer_size)) {
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  base::UnsafeSharedMemoryRegion output_region =
      mojo::UnwrapUnsafeSharedMemoryRegion(std::move(output_handle));
  DCHECK(output_region.IsValid());
  DCHECK_GE(output_region.GetSize(), output_buffer_size);

  base::WritableSharedMemoryMapping mapping =
      output_region.MapAt(0, output_buffer_size);
  if (!mapping.IsValid()) {
    LOG(ERROR) << "Could not map output shared memory for input buffer id "
               << input_buffer.id();
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }

  uint8_t* shm_memory = mapping.GetMemoryAsSpan<uint8_t>().data();
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_I420,  // format
      coded_size,                // coded_size
      gfx::Rect(coded_size),     // visible_rect
      coded_size,                // natural_size
      shm_memory,                // data
      output_buffer_size,        // data_size
      base::TimeDelta());        // timestamp
  if (!frame.get()) {
    LOG(ERROR) << "Could not create VideoFrame for input buffer id "
               << input_buffer.id();
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }
  frame->BackWithOwnedSharedMemory(std::move(output_region),
                                   std::move(mapping));

  DCHECK(accelerator_);
  accelerator_->Decode(std::move(input_buffer), frame);
}

void MojoMjpegDecodeAcceleratorService::DecodeWithFD(
    int32_t buffer_id,
    mojo::ScopedHandle input_handle,
    uint32_t input_buffer_size,
    int32_t coded_size_width,
    int32_t coded_size_height,
    mojo::ScopedHandle output_handle,
    uint32_t output_buffer_size,
    DecodeWithFDCallback callback) {
#if defined(OS_CHROMEOS)
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::PlatformFile input_fd;
  base::PlatformFile output_fd;
  MojoResult result;

  result = mojo::UnwrapPlatformFile(std::move(input_handle), &input_fd);
  if (result != MOJO_RESULT_OK) {
    std::move(callback).Run(
        buffer_id,
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }

  result = mojo::UnwrapPlatformFile(std::move(output_handle), &output_fd);
  if (result != MOJO_RESULT_OK) {
    std::move(callback).Run(
        buffer_id,
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }

  base::subtle::PlatformSharedMemoryRegion input_shm_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          base::ScopedFD(input_fd),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
          input_buffer_size, base::UnguessableToken::Create());
  media::BitstreamBuffer in_buffer(buffer_id, std::move(input_shm_region),
                                   input_buffer_size);
  gfx::Size coded_size(coded_size_width, coded_size_height);

  base::subtle::PlatformSharedMemoryRegion output_shm_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          base::ScopedFD(output_fd),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
          output_buffer_size, base::UnguessableToken::Create());
  mojo::ScopedSharedBufferHandle output_scoped_handle =
      mojo::WrapPlatformSharedMemoryRegion(std::move(output_shm_region));

  Decode(std::move(in_buffer), coded_size, std::move(output_scoped_handle),
         output_buffer_size, std::move(callback));
#else
  NOTREACHED();
#endif
}

void MojoMjpegDecodeAcceleratorService::Uninitialize() {
  // TODO(c.padhi): see http://crbug.com/699255.
  NOTIMPLEMENTED();
}

void MojoMjpegDecodeAcceleratorService::NotifyDecodeStatus(
    int32_t bitstream_buffer_id,
    ::chromeos_camera::MjpegDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto iter = decode_cb_map_.find(bitstream_buffer_id);
  DCHECK(iter != decode_cb_map_.end());
  if (iter == decode_cb_map_.end()) {
    // Silently ignoring abnormal case.
    return;
  }

  DecodeCallback decode_cb = std::move(iter->second);
  decode_cb_map_.erase(iter);
  std::move(decode_cb).Run(bitstream_buffer_id, error);
}

}  // namespace chromeos_camera
