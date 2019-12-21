// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl3.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/color_space.h"

namespace blink {

namespace {

gfx::ColorSpace::PrimaryID GetPrimaryID(CanvasColorSpace color_space) {
  gfx::ColorSpace::PrimaryID primary_id = gfx::ColorSpace::PrimaryID::BT709;
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
    case CanvasColorSpace::kLinearRGB:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      break;
    case CanvasColorSpace::kRec2020:
      primary_id = gfx::ColorSpace::PrimaryID::BT2020;
      break;
    case CanvasColorSpace::kP3:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTEST432_1;
      break;
  }
  return primary_id;
}

}  // namespace

CanvasColorParams::CanvasColorParams() = default;

CanvasColorParams::CanvasColorParams(CanvasColorSpace color_space,
                                     CanvasPixelFormat pixel_format,
                                     OpacityMode opacity_mode)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode) {}

CanvasColorParams::CanvasColorParams(const SkImageInfo& info)
    : CanvasColorParams(info.refColorSpace(), info.colorType()) {}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpaceForSkSurfaces() const {
  return GetSkColorSpace();
}

bool CanvasColorParams::NeedsColorConversion(
    const CanvasColorParams& dest_color_params) const {
  if ((color_space_ == dest_color_params.ColorSpace() &&
       pixel_format_ == dest_color_params.PixelFormat()))
    return false;
  return true;
}

SkColorType CanvasColorParams::GetSkColorType() const {
  switch (pixel_format_) {
    case CanvasPixelFormat::kF16:
      return kRGBA_F16_SkColorType;
    case CanvasPixelFormat::kRGBA8:
      return kRGBA_8888_SkColorType;
    case CanvasPixelFormat::kBGRA8:
      return kBGRA_8888_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}

SkAlphaType CanvasColorParams::GetSkAlphaType() const {
  return opacity_mode_ == kOpaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
}

const SkSurfaceProps* CanvasColorParams::GetSkSurfaceProps() const {
  static const SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  if (opacity_mode_ == kOpaque)
    return nullptr;
  return &disable_lcd_props;
}

uint8_t CanvasColorParams::BytesPerPixel() const {
  return SkColorTypeBytesPerPixel(GetSkColorType());
}

gfx::ColorSpace CanvasColorParams::GetSamplerGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primary_id = GetPrimaryID(color_space_);

  // TODO(ccameron): This needs to take into account whether or not this texture
  // will be sampled in linear or nonlinear space.
  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::IEC61966_2_1;
  if (pixel_format_ == CanvasPixelFormat::kF16)
    transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;

  return gfx::ColorSpace(primary_id, transfer_id);
}

gfx::ColorSpace CanvasColorParams::GetStorageGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primary_id = GetPrimaryID(color_space_);

  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::IEC61966_2_1;
  // Only sRGB and e-sRGB use sRGB transfer function. Other canvas color spaces,
  // i.e., linear-rgb, p3 and rec2020 use linear transfer function.
  if (color_space_ != CanvasColorSpace::kSRGB)
    transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;

  return gfx::ColorSpace(primary_id, transfer_id);
}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpace() const {
  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "Unexpected kN32_SkColorType value.");
  skcms_Matrix3x3 gamut = SkNamedGamut::kSRGB;
  skcms_TransferFunction transferFn = SkNamedTransferFn::kSRGB;
  switch (color_space_) {
    case CanvasColorSpace::kSRGB:
      break;
    case CanvasColorSpace::kLinearRGB:
      transferFn = SkNamedTransferFn::kLinear;
      break;
    case CanvasColorSpace::kRec2020:
      gamut = SkNamedGamut::kRec2020;
      transferFn = SkNamedTransferFn::kLinear;
      break;
    case CanvasColorSpace::kP3:
      gamut = SkNamedGamut::kDCIP3;
      transferFn = SkNamedTransferFn::kLinear;
      break;
  }
  return SkColorSpace::MakeRGB(transferFn, gamut);
}

gfx::BufferFormat CanvasColorParams::GetBufferFormat() const {
  switch (GetSkColorType()) {
    case kRGBA_8888_SkColorType:
      return gfx::BufferFormat::RGBA_8888;
    case kBGRA_8888_SkColorType:
      return gfx::BufferFormat::BGRA_8888;
    case kRGBA_F16_SkColorType:
      return gfx::BufferFormat::RGBA_F16;
    default:
      NOTREACHED();
  }

  return gfx::BufferFormat::RGBA_8888;
}

GLenum CanvasColorParams::GLUnsizedInternalFormat() const {
  // TODO(junov): try GL_RGB when opacity_mode_ == kOpaque
  switch (GetSkColorType()) {
    case kRGBA_8888_SkColorType:
      return GL_RGBA;
    case kBGRA_8888_SkColorType:
      return GL_BGRA_EXT;
    case kRGBA_F16_SkColorType:
      return GL_RGBA;
    default:
      NOTREACHED();
  }

  return GL_RGBA;
}

GLenum CanvasColorParams::GLSizedInternalFormat() const {
  switch (GetSkColorType()) {
    case kRGBA_8888_SkColorType:
      return GL_RGBA8;
    case kBGRA_8888_SkColorType:
      return GL_BGRA8_EXT;
    case kRGBA_F16_SkColorType:
      return GL_RGBA16F;
    default:
      NOTREACHED();
  }

  return GL_RGBA8;
}

GLenum CanvasColorParams::GLType() const {
  switch (GetSkColorType()) {
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
      return GL_UNSIGNED_BYTE;
    case kRGBA_F16_SkColorType:
      return GL_HALF_FLOAT_OES;
    default:
      NOTREACHED();
  }

  return GL_UNSIGNED_BYTE;
}

viz::ResourceFormat CanvasColorParams::TransferableResourceFormat() const {
  return viz::GetResourceFormat(GetBufferFormat());
}

CanvasColorParams::CanvasColorParams(const sk_sp<SkColorSpace> color_space,
                                     SkColorType color_type) {
  color_space_ = CanvasColorSpace::kSRGB;
  pixel_format_ = GetNativeCanvasPixelFormat();
  // When there is no color space information, the SkImage is in legacy mode and
  // the color type is kRGBA8 canvas pixel format.
  if (!color_space)
    return;

  // CanvasColorSpace::kSRGB covers sRGB and e-sRGB. We need to check for
  // linear-rgb, rec2020 and p3.
  if (SkColorSpace::Equals(color_space.get(),
                           SkColorSpace::MakeSRGB()->makeLinearGamma().get())) {
    color_space_ = CanvasColorSpace::kLinearRGB;
  } else if (SkColorSpace::Equals(
                 color_space.get(),
                 SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                                       SkNamedGamut::kRec2020)
                     .get())) {
    color_space_ = CanvasColorSpace::kRec2020;
  } else if (SkColorSpace::Equals(
                 color_space.get(),
                 SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                                       SkNamedGamut::kDCIP3)
                     .get())) {
    color_space_ = CanvasColorSpace::kP3;
  }

  if (color_type == kRGBA_F16_SkColorType)
    pixel_format_ = CanvasPixelFormat::kF16;
  else if (color_type == kRGBA_8888_SkColorType)
    pixel_format_ = CanvasPixelFormat::kRGBA8;
}

}  // namespace blink
