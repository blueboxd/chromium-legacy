// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_color_params.h"

namespace blink {

SerializedColorParams::SerializedColorParams()
    : color_space_(SerializedColorSpace::kSRGB),
      pixel_format_(SerializedPixelFormat::kRGBA8),
      opacity_mode_(SerializedOpacityMode::kNonOpaque),
      storage_format_(SerializedImageDataStorageFormat::kUint8Clamped) {}

SerializedColorParams::SerializedColorParams(CanvasColorParams color_params) {
  switch (color_params.ColorSpace()) {
    case CanvasColorSpace::kSRGB:
      color_space_ = SerializedColorSpace::kSRGB;
      break;
    case CanvasColorSpace::kLinearRGB:
      color_space_ = SerializedColorSpace::kLinearRGB;
      break;
    case CanvasColorSpace::kRec2020:
      color_space_ = SerializedColorSpace::kRec2020;
      break;
    case CanvasColorSpace::kP3:
      color_space_ = SerializedColorSpace::kP3;
      break;
  }
  // todo(crbug/1021986) remove force_rgba in canvasColorParams
  if (color_params.GetForceRGBA() == CanvasForceRGBA::kForced) {
    pixel_format_ = SerializedPixelFormat::kForceRGBA8;
  } else {
    switch (color_params.PixelFormat()) {
      case CanvasPixelFormat::kRGBA8:
        pixel_format_ = SerializedPixelFormat::kRGBA8;
        break;
      case CanvasPixelFormat::kF16:
        pixel_format_ = SerializedPixelFormat::kF16;
        break;
    }
  }

  opacity_mode_ = SerializedOpacityMode::kNonOpaque;
  if (color_params.GetOpacityMode() == blink::kOpaque)
    opacity_mode_ = SerializedOpacityMode::kOpaque;
  storage_format_ = SerializedImageDataStorageFormat::kUint8Clamped;
}

SerializedColorParams::SerializedColorParams(
    CanvasColorParams color_params,
    ImageDataStorageFormat storage_format)
    : SerializedColorParams(color_params) {
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kUint8Clamped;
      break;
    case kUint16ArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kUint16;
      break;
    case kFloat32ArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kFloat32;
      break;
  }
}

SerializedColorParams::SerializedColorParams(
    SerializedColorSpace color_space,
    SerializedPixelFormat pixel_format,
    SerializedOpacityMode opacity_mode,
    SerializedImageDataStorageFormat storage_format) {
  SetSerializedColorSpace(color_space);
  SetSerializedPixelFormat(pixel_format);
  SetSerializedOpacityMode(opacity_mode);
  SetSerializedImageDataStorageFormat(storage_format);
}

CanvasColorParams SerializedColorParams::GetCanvasColorParams() const {
  CanvasColorSpace color_space = CanvasColorSpace::kSRGB;
  switch (color_space_) {
    case SerializedColorSpace::kLegacyObsolete:
    case SerializedColorSpace::kSRGB:
      color_space = CanvasColorSpace::kSRGB;
      break;
    case SerializedColorSpace::kLinearRGB:
      color_space = CanvasColorSpace::kLinearRGB;
      break;
    case SerializedColorSpace::kRec2020:
      color_space = CanvasColorSpace::kRec2020;
      break;
    case SerializedColorSpace::kP3:
      color_space = CanvasColorSpace::kP3;
      break;
  }

  // todo(crbug/1021986) remove force_rgba in canvasColorParams
  CanvasForceRGBA force_rgba = CanvasForceRGBA::kNotForced;
  CanvasPixelFormat pixel_format = CanvasPixelFormat::kRGBA8;
  if (pixel_format_ == SerializedPixelFormat::kForceRGBA8) {
    force_rgba = CanvasForceRGBA::kForced;
  } else if (pixel_format_ == SerializedPixelFormat::kF16) {
    pixel_format = CanvasPixelFormat::kF16;
  } else if (pixel_format_ == SerializedPixelFormat::kRGBA8) {
    pixel_format = CanvasPixelFormat::kRGBA8;
  }

  blink::OpacityMode opacity_mode = blink::kNonOpaque;
  if (opacity_mode_ == SerializedOpacityMode::kOpaque)
    opacity_mode = blink::kOpaque;

  return CanvasColorParams(color_space, pixel_format, opacity_mode, force_rgba);
}

CanvasColorSpace SerializedColorParams::GetColorSpace() const {
  return GetCanvasColorParams().ColorSpace();
}

ImageDataStorageFormat SerializedColorParams::GetStorageFormat() const {
  switch (storage_format_) {
    case SerializedImageDataStorageFormat::kUint8Clamped:
      return kUint8ClampedArrayStorageFormat;
    case SerializedImageDataStorageFormat::kUint16:
      return kUint16ArrayStorageFormat;
    case SerializedImageDataStorageFormat::kFloat32:
      return kFloat32ArrayStorageFormat;
  }
  NOTREACHED();
  return kUint8ClampedArrayStorageFormat;
}

void SerializedColorParams::SetSerializedColorSpace(
    SerializedColorSpace color_space) {
  color_space_ = color_space;
}

void SerializedColorParams::SetSerializedPixelFormat(
    SerializedPixelFormat pixel_format) {
  pixel_format_ = pixel_format;
}

void SerializedColorParams::SetSerializedOpacityMode(
    SerializedOpacityMode opacity_mode) {
  opacity_mode_ = opacity_mode;
}

void SerializedColorParams::SetSerializedImageDataStorageFormat(
    SerializedImageDataStorageFormat storage_format) {
  storage_format_ = storage_format;
}

SerializedColorSpace SerializedColorParams::GetSerializedColorSpace() const {
  return color_space_;
}

SerializedPixelFormat SerializedColorParams::GetSerializedPixelFormat() const {
  return pixel_format_;
}

SerializedOpacityMode SerializedColorParams::GetSerializedOpacityMode() const {
  return opacity_mode_;
}

SerializedImageDataStorageFormat
SerializedColorParams::GetSerializedImageDataStorageFormat() const {
  return storage_format_;
}

}  // namespace blink
