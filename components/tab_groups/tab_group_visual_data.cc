// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/tab_group_visual_data.h"

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/tab_groups/tab_group_color.h"

namespace tab_groups {

TabGroupVisualData::TabGroupVisualData()
    : TabGroupVisualData(base::string16(), TabGroupColorId::kGrey) {}

TabGroupVisualData::TabGroupVisualData(base::string16 title,
                                       tab_groups::TabGroupColorId color)
    : title_(std::move(title)), color_(color) {}

TabGroupVisualData::TabGroupVisualData(base::string16 title, uint32_t color_int)
    : title_(std::move(title)), color_(TabGroupColorId::kGrey) {
  auto color_id = static_cast<tab_groups::TabGroupColorId>(color_int);
  if (base::Contains(tab_groups::GetTabGroupColorLabelMap(), color_id))
    color_ = color_id;
}

}  // namespace tab_groups
