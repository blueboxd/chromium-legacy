// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/tab_group_visual_data.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/tab_groups/tab_group_color.h"

namespace tab_groups {

TabGroupVisualData::TabGroupVisualData() {
  title_ = base::ASCIIToUTF16("");
  color_ = tab_groups::TabGroupColorId::kGrey;
}

TabGroupVisualData::TabGroupVisualData(base::string16 title,
                                       tab_groups::TabGroupColorId color)
    : title_(title), color_(color) {}

}  // namespace tab_groups
