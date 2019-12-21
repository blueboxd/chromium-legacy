// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/tab_group_color.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/utils/SkRandom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"

namespace tab_groups {

const base::flat_map<TabGroupColorId, TabGroupColor>& GetTabGroupColorSet() {
  static const base::NoDestructor<
      base::flat_map<TabGroupColorId, TabGroupColor>>
      kTabGroupColors(
          {{TabGroupColorId::kGrey,
            TabGroupColor{gfx::kGoogleGrey700, gfx::kGoogleGrey400,
                          l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_GREY)}},
           {TabGroupColorId::kBlue,
            TabGroupColor{gfx::kGoogleBlue600, gfx::kGoogleBlue300,
                          l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_BLUE)}},
           {TabGroupColorId::kRed,
            TabGroupColor{gfx::kGoogleRed600, gfx::kGoogleRed300,
                          l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_RED)}},
           {TabGroupColorId::kYellow,
            TabGroupColor{
                gfx::kGoogleYellow900, gfx::kGoogleYellow300,
                l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_YELLOW)}},
           {TabGroupColorId::kGreen,
            TabGroupColor{
                gfx::kGoogleGreen600, gfx::kGoogleGreen300,
                l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_GREEN)}},
           {TabGroupColorId::kPink,
            TabGroupColor{gfx::kGooglePink700, gfx::kGooglePink300,
                          l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_PINK)}},
           {TabGroupColorId::kPurple,
            TabGroupColor{
                gfx::kGooglePurple600, gfx::kGooglePurple200,
                l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_PURPLE)}},
           {TabGroupColorId::kCyan,
            TabGroupColor{
                gfx::kGoogleCyan900, gfx::kGoogleCyan300,
                l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_CYAN)}}});
  return *kTabGroupColors;
}

}  // namespace tab_groups
