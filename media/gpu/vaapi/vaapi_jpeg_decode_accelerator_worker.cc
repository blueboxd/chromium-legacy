// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_decode_accelerator_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VAJDAWorkerDecoderFailure {
  kVaapiError = 0,
  kMaxValue = kVaapiError,
};

void ReportToVAJDAWorkerDecoderFailureUMA(VAJDAWorkerDecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDAWorker.DecoderFailure", failure);
}

// Uses |decoder| to decode the JPEG corresponding to |encoded_data|.
// |decode_cb| is called when finished or when an error is encountered. We don't
// support decoding to scale, so |output_size| is only used for tracing.
void DecodeTask(
    VaapiImageDecoder* decoder,
    std::vector<uint8_t> encoded_data,
    const gfx::Size& output_size,
    gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB decode_cb) {
  TRACE_EVENT2("jpeg", "VaapiJpegDecodeAcceleratorWorker::DecodeTask",
               "encoded_bytes", encoded_data.size(), "output_size",
               output_size.ToString());
  gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB scoped_decode_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(decode_cb),
                                                  nullptr);

  // Decode into a VAAPI surface.
  DCHECK(decoder);
  VaapiImageDecodeStatus status = decoder->Decode(
      base::make_span<const uint8_t>(encoded_data.data(), encoded_data.size()));
  if (status != VaapiImageDecodeStatus::kSuccess) {
    DVLOGF(1) << "Failed to decode - status = "
              << static_cast<uint32_t>(status);
    return;
  }

  // Export the decode result as a NativePixmap.
  std::unique_ptr<NativePixmapAndSizeInfo> exported_pixmap =
      decoder->ExportAsNativePixmapDmaBuf(&status);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    DVLOGF(1) << "Failed to export surface - status = "
              << static_cast<uint32_t>(status);
    return;
  }
  DCHECK(exported_pixmap);
  DCHECK(exported_pixmap->pixmap);
  if (exported_pixmap->pixmap->GetBufferSize() != output_size) {
    DVLOGF(1) << "Scaling is not supported";
    return;
  }

  // Output the decoded data.
  gfx::NativePixmapHandle pixmap_handle =
      exported_pixmap->pixmap->ExportHandle();
  // If a dup() failed while exporting the handle, we would get no planes.
  if (pixmap_handle.planes.empty()) {
    DVLOGF(1) << "Could not export the NativePixmapHandle";
    return;
  }
  auto result =
      std::make_unique<gpu::ImageDecodeAcceleratorWorker::DecodeResult>();
  result->handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
  result->handle.native_pixmap_handle = std::move(pixmap_handle);
  result->visible_size = exported_pixmap->pixmap->GetBufferSize();
  result->buffer_format = exported_pixmap->pixmap->GetBufferFormat();
  result->buffer_byte_size = exported_pixmap->byte_size;
  std::move(scoped_decode_callback).Run(std::move(result));
}

}  // namespace

// static
std::unique_ptr<VaapiJpegDecodeAcceleratorWorker>
VaapiJpegDecodeAcceleratorWorker::Create() {
  auto decoder = std::make_unique<VaapiJpegDecoder>();
  if (!decoder->Initialize(
          base::BindRepeating(&ReportToVAJDAWorkerDecoderFailureUMA,
                              VAJDAWorkerDecoderFailure::kVaapiError))) {
    return nullptr;
  }
  return base::WrapUnique(
      new VaapiJpegDecodeAcceleratorWorker(std::move(decoder)));
}

VaapiJpegDecodeAcceleratorWorker::VaapiJpegDecodeAcceleratorWorker(
    std::unique_ptr<VaapiJpegDecoder> decoder)
    : decoder_(std::move(decoder)) {
  DCHECK(decoder_);
  decoder_task_runner_ = base::CreateSequencedTaskRunnerWithTraits({});
  DCHECK(decoder_task_runner_);
}

VaapiJpegDecodeAcceleratorWorker::~VaapiJpegDecodeAcceleratorWorker() {
  if (decoder_task_runner_)
    decoder_task_runner_->DeleteSoon(FROM_HERE, std::move(decoder_));
}

std::vector<gpu::ImageDecodeAcceleratorSupportedProfile>
VaapiJpegDecodeAcceleratorWorker::GetSupportedProfiles() {
  DCHECK(decoder_);
  const gpu::ImageDecodeAcceleratorSupportedProfile supported_profile =
      decoder_->GetSupportedProfile();
  DCHECK_EQ(gpu::ImageDecodeAcceleratorType::kJpeg,
            supported_profile.image_type);
  return {supported_profile};
}

void VaapiJpegDecodeAcceleratorWorker::Decode(std::vector<uint8_t> encoded_data,
                                              const gfx::Size& output_size,
                                              CompletedDecodeCB decode_cb) {
  DCHECK(decoder_task_runner_);
  DCHECK(!decoder_task_runner_->RunsTasksInCurrentSequence());
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DecodeTask, decoder_.get(), std::move(encoded_data),
                     output_size, std::move(decode_cb)));
}

}  // namespace media
