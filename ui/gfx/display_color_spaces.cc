// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/display_color_spaces.h"

namespace gfx {

namespace {

size_t GetIndex(ContentColorUsage color_usage, bool needs_alpha) {
  switch (color_usage) {
    case ContentColorUsage::kSRGB:
      return 3 * needs_alpha;
    case ContentColorUsage::kWideColorGamut:
      return 3 * needs_alpha + 1;
    case ContentColorUsage::kHDR:
      return 3 * needs_alpha + 2;
  }
}

}  // namespace

DisplayColorSpaces::DisplayColorSpaces() {
  for (auto& color_space : color_spaces_)
    color_space = gfx::ColorSpace::CreateSRGB();
  for (auto& buffer_format : buffer_formats_)
    buffer_format = gfx::BufferFormat::RGBA_8888;
}

DisplayColorSpaces::DisplayColorSpaces(const gfx::ColorSpace& c)
    : DisplayColorSpaces() {
  if (!c.IsValid())
    return;
  for (auto& color_space : color_spaces_)
    color_space = c;
}

DisplayColorSpaces::DisplayColorSpaces(const ColorSpace& c, BufferFormat f) {
  for (auto& color_space : color_spaces_)
    color_space = c;
  for (auto& buffer_format : buffer_formats_)
    buffer_format = f;
}

void DisplayColorSpaces::SetOutputColorSpaceAndBufferFormat(
    ContentColorUsage color_usage,
    bool needs_alpha,
    const gfx::ColorSpace& color_space,
    gfx::BufferFormat buffer_format) {
  size_t i = GetIndex(color_usage, needs_alpha);
  color_spaces_[i] = color_space;
  buffer_formats_[i] = buffer_format;
}

ColorSpace DisplayColorSpaces::GetOutputColorSpace(
    ContentColorUsage color_usage,
    bool needs_alpha) const {
  return color_spaces_[GetIndex(color_usage, needs_alpha)];
}

BufferFormat DisplayColorSpaces::GetOutputBufferFormat(
    ContentColorUsage color_usage,
    bool needs_alpha) const {
  return buffer_formats_[GetIndex(color_usage, needs_alpha)];
}

gfx::ColorSpace DisplayColorSpaces::GetRasterColorSpace() const {
  return GetOutputColorSpace(ContentColorUsage::kHDR, false /* needs_alpha */)
      .GetRasterColorSpace();
}

gfx::ColorSpace DisplayColorSpaces::GetCompositingColorSpace(
    bool needs_alpha,
    ContentColorUsage color_usage) const {
  gfx::ColorSpace result = GetOutputColorSpace(color_usage, needs_alpha);
  if (!result.IsSuitableForBlending())
    result = gfx::ColorSpace::CreateExtendedSRGB();
  return result;
}

bool DisplayColorSpaces::SupportsHDR() const {
  return GetOutputColorSpace(ContentColorUsage::kHDR, false).IsHDR() ||
         GetOutputColorSpace(ContentColorUsage::kHDR, true).IsHDR();
}

bool DisplayColorSpaces::operator==(const DisplayColorSpaces& other) const {
  for (size_t i = 0; i < kConfigCount; ++i) {
    if (color_spaces_[i] != other.color_spaces_[i])
      return false;
    if (buffer_formats_[i] != other.buffer_formats_[i])
      return false;
  }
  if (sdr_white_level_ != other.sdr_white_level_)
    return false;

  return true;
}

bool DisplayColorSpaces::operator!=(const DisplayColorSpaces& other) const {
  return !(*this == other);
}

}  // namespace gfx
