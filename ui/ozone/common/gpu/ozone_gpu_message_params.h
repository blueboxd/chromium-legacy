// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_
#define UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

struct DisplayMode_Params {
  DisplayMode_Params();
  ~DisplayMode_Params();

  gfx::Size size;
  bool is_interlaced = false;
  float refresh_rate = 0.0f;
};

struct DisplaySnapshot_Params {
  DisplaySnapshot_Params();
  DisplaySnapshot_Params(const DisplaySnapshot_Params& other);
  ~DisplaySnapshot_Params();

  int64_t display_id = 0;
  gfx::Point origin;
  gfx::Size physical_size;
  display::DisplayConnectionType type = display::DISPLAY_CONNECTION_TYPE_NONE;
  bool is_aspect_preserving_scaling = false;
  bool has_overscan = false;
  display::PrivacyScreenState privacy_screen_state =
      display::PrivacyScreenState::kNotSupported;
  bool has_color_correction_matrix = false;
  bool color_correction_in_linear_space = false;
  gfx::ColorSpace color_space;
  uint32_t bits_per_channel = 0;
  std::string display_name;
  base::FilePath sys_path;
  std::vector<DisplayMode_Params> modes;
  display::PanelOrientation panel_orientation =
      display::PanelOrientation::kNormal;
  std::vector<uint8_t> edid;
  bool has_current_mode = false;
  DisplayMode_Params current_mode;
  bool has_native_mode = false;
  DisplayMode_Params native_mode;
  int64_t product_code = 0;
  int32_t year_of_manufacture = display::kInvalidYearOfManufacture;
  gfx::Size maximum_cursor_size;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_
