// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_configuration_params.h"

namespace display {

DisplayConfigurationParams::DisplayConfigurationParams() = default;

DisplayConfigurationParams::DisplayConfigurationParams(
    int64_t id,
    gfx::Point origin,
    std::unique_ptr<display::DisplayMode> pmode)
    : id(id), origin(origin), mode(std::move(pmode)) {}

DisplayConfigurationParams::~DisplayConfigurationParams() = default;

}  // namespace display
