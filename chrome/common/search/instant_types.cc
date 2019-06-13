// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/search/instant_types.h"

ThemeBackgroundInfo::ThemeBackgroundInfo()
    : using_default_theme(true),
      using_dark_mode(false),
      custom_background_url(std::string()),
      custom_background_attribution_line_1(std::string()),
      custom_background_attribution_line_2(std::string()),
      custom_background_attribution_action_url(std::string()),
      background_color(),
      text_color(),
      text_color_light(),
      image_horizontal_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_vertical_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_tiling(THEME_BKGRND_IMAGE_NO_REPEAT),
      has_attribution(false),
      logo_alternate(false) {}

ThemeBackgroundInfo::~ThemeBackgroundInfo() {
}

bool ThemeBackgroundInfo::operator==(const ThemeBackgroundInfo& rhs) const {
  return using_default_theme == rhs.using_default_theme &&
         using_dark_mode == rhs.using_dark_mode &&
         custom_background_url == rhs.custom_background_url &&
         custom_background_attribution_line_1 ==
             rhs.custom_background_attribution_line_1 &&
         custom_background_attribution_line_2 ==
             rhs.custom_background_attribution_line_2 &&
         custom_background_attribution_action_url ==
             rhs.custom_background_attribution_action_url &&
         background_color == rhs.background_color &&
         text_color == rhs.text_color &&
         text_color_light == rhs.text_color_light && theme_id == rhs.theme_id &&
         image_horizontal_alignment == rhs.image_horizontal_alignment &&
         image_vertical_alignment == rhs.image_vertical_alignment &&
         image_tiling == rhs.image_tiling &&
         has_attribution == rhs.has_attribution &&
         logo_alternate == rhs.logo_alternate;
}

InstantMostVisitedItem::InstantMostVisitedItem()
    : title_source(ntp_tiles::TileTitleSource::UNKNOWN),
      source(ntp_tiles::TileSource::TOP_SITES) {}

InstantMostVisitedItem::InstantMostVisitedItem(
    const InstantMostVisitedItem& other) = default;

InstantMostVisitedItem::~InstantMostVisitedItem() {}

InstantMostVisitedInfo::InstantMostVisitedInfo()
    : items_are_custom_links(false),
      use_most_visited(false),
      is_visible(true) {}

InstantMostVisitedInfo::InstantMostVisitedInfo(
    const InstantMostVisitedInfo& other) = default;

InstantMostVisitedInfo::~InstantMostVisitedInfo() {}
