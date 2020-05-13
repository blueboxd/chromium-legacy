// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_source_base.h"

namespace ui {

WaylandDataSourceBase::WaylandDataSourceBase() = default;
WaylandDataSourceBase::~WaylandDataSourceBase() = default;

void WaylandDataSourceBase::GetClipboardData(
    const std::string& mime_type,
    base::Optional<std::vector<uint8_t>>* data) const {
  auto it = data_map_.find(mime_type);
  if (it == data_map_.end())
    return;
  data->emplace(it->second);
}

}  // namespace ui
