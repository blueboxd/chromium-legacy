// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

namespace ui {

DisplayMode_Params::DisplayMode_Params() {}

DisplayMode_Params::~DisplayMode_Params() {}

DisplaySnapshot_Params::DisplaySnapshot_Params() {}

DisplaySnapshot_Params::DisplaySnapshot_Params(
    const DisplaySnapshot_Params& other) = default;

DisplaySnapshot_Params::~DisplaySnapshot_Params() {}

}  // namespace ui
