// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

namespace {
static constexpr uint64_t kDawnRowPitchAlignmentBits = 8;

// Calculate row pitch for T2B/B2T copy
// TODO(shaobo.yan@intel.com): Using Dawn's constants once they are exposed
uint64_t AlignWebGPURowPitch(uint64_t bytesPerRow) {
  return (((bytesPerRow - 1) >> kDawnRowPitchAlignmentBits) + 1)
         << kDawnRowPitchAlignmentBits;
}

}  // anonymous namespace

WebGPUImageUploadSizeInfo ComputeImageBitmapWebGPUUploadSizeInfo(
    const IntRect& rect,
    const CanvasColorParams& color_params) {
  WebGPUImageUploadSizeInfo info;
  uint64_t bytes_per_pixel = color_params.BytesPerPixel();

  uint64_t row_pitch = AlignWebGPURowPitch(rect.Width() * bytes_per_pixel);

  // Currently, row pitch for buffer copy view in WebGPU is an uint32_t type
  // value and the maximum value is std::numeric_limits<uint32_t>::max().
  DCHECK(row_pitch <= std::numeric_limits<uint32_t>::max());

  info.wgpu_row_pitch = static_cast<uint32_t>(row_pitch);
  info.size_in_bytes = row_pitch * rect.Height();

  return info;
}

bool CopyBytesFromImageBitmapForWebGPU(scoped_refptr<StaticBitmapImage> image,
                                       base::span<uint8_t> dst,
                                       const IntRect& rect,
                                       const CanvasColorParams& color_params) {
  DCHECK(image);
  DCHECK_GT(dst.size(), static_cast<size_t>(0));
  DCHECK(image->width() - rect.X() >= rect.Width());
  DCHECK(image->height() - rect.Y() >= rect.Height());

  WebGPUImageUploadSizeInfo wgpu_info =
      ComputeImageBitmapWebGPUUploadSizeInfo(rect, color_params);
  DCHECK_EQ(static_cast<uint64_t>(dst.size()), wgpu_info.size_in_bytes);

  // Prepare extract data from SkImage.
  // TODO(shaobo.yan@intel.com): Make sure the data is in the correct format for
  // copying to WebGPU
  SkColorType color_type =
      (color_params.GetSkColorType() == kRGBA_F16_SkColorType)
          ? kRGBA_F16_SkColorType
          : kRGBA_8888_SkColorType;

  // Read pixel request dst info.
  // TODO(shaobo.yan@intel.com): Use Skia to do transform and color conversion.
  SkImageInfo info = SkImageInfo::Make(
      rect.Width(), rect.Height(), color_type, kUnpremul_SkAlphaType,
      color_params.GetSkColorSpaceForSkSurfaces());

  sk_sp<SkImage> sk_image = image->PaintImageForCurrentFrame().GetSkImage();

  if (!sk_image)
    return false;

  bool read_pixels_successful = sk_image->readPixels(
      info, dst.data(), wgpu_info.wgpu_row_pitch, rect.X(), rect.Y());

  if (!read_pixels_successful) {
    return false;
  }

  return true;
}

DawnTextureFromImageBitmap::DawnTextureFromImageBitmap(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    uint64_t device_client_id)
    : dawn_control_client_(dawn_control_client),
      device_client_id_(device_client_id) {}

DawnTextureFromImageBitmap::~DawnTextureFromImageBitmap() {
  // Ensure calls to ProduceDawnTextureFromImageBitmap
  // and FinishDawnTextureFromImageBitmapAccess are matched.
  DCHECK_EQ(wire_texture_id_, 0u);
  DCHECK_EQ(wire_texture_generation_, 0u);

  device_client_id_ = 0;
  dawn_control_client_.reset();
}

WGPUTexture DawnTextureFromImageBitmap::ProduceDawnTextureFromImageBitmap(
    scoped_refptr<StaticBitmapImage> image) {
  DCHECK(!dawn_control_client_->IsDestroyed());

  associated_resource_ = image->GetMailboxHolder().mailbox;

  // Produce and inject image to WebGPU texture
  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
  gpu::webgpu::ReservedTexture reservation =
      webgpu->ReserveTexture(device_client_id_);
  DCHECK(reservation.texture);

  wire_texture_id_ = reservation.id;
  wire_texture_generation_ = reservation.generation;

  // This may fail because gl_backing resource cannot produce dawn
  // representation.
  webgpu->AssociateMailbox(device_client_id_, 0, wire_texture_id_,
                           wire_texture_generation_, WGPUTextureUsage_CopySrc,
                           reinterpret_cast<GLbyte*>(&associated_resource_));

  return reservation.texture;
}

void DawnTextureFromImageBitmap::FinishDawnTextureFromImageBitmapAccess() {
  DCHECK(!dawn_control_client_->IsDestroyed());
  DCHECK_NE(wire_texture_id_, 0u);

  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
  webgpu->DissociateMailbox(device_client_id_, wire_texture_id_,
                            wire_texture_generation_);
  wire_texture_id_ = 0;
  wire_texture_generation_ = 0;
  associated_resource_.SetZero();
}
}  // namespace blink
