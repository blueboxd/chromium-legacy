// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/gfx/color_palette.h"

namespace app_list {

namespace {

// The height reduced from the tile when min scale is not sufficient to make the
// apps grid fit the available size - This would essentially remove the vertical
// padding for the unclipped folder icon.
constexpr int kMinYScaleHeightAdjustment = 16;

// Scales |value| using the smaller one of |scale_1| and |scale_2|.
int MinScale(int value, float scale_1, float scale_2) {
  return std::round(value * std::min(scale_1, scale_2));
}

int GridTileWidthForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
      return 112;
    case ash::AppListConfigType::kLarge:
      return 120;
    case ash::AppListConfigType::kMedium:
      return 88;
    case ash::AppListConfigType::kSmall:
      return 80;
  }

  NOTREACHED();
  return GridTileWidthForType(ash::AppListConfigType::kShared);
}

int GridTileHeightForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 120;
    case ash::AppListConfigType::kMedium:
      return 88;
    case ash::AppListConfigType::kSmall:
      return 80;
  }

  NOTREACHED();
  return GridTileHeightForType(ash::AppListConfigType::kShared);
}

int GridIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 64;
    case ash::AppListConfigType::kMedium:
      return 48;
    case ash::AppListConfigType::kSmall:
      return 40;
  }

  NOTREACHED();
  return GridIconDimensionForType(ash::AppListConfigType::kShared);
}

int GridTitleTopPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 92;
    case ash::AppListConfigType::kMedium:
      return 64;
    case ash::AppListConfigType::kSmall:
      return 56;
  }

  NOTREACHED();
  return GridTitleTopPaddingForType(ash::AppListConfigType::kShared);
}

int GridTitleBottomPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 8;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
      return 6;
  }

  NOTREACHED();
  return GridTitleBottomPaddingForType(ash::AppListConfigType::kShared);
}

int GridTitleHorizontalPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 8;
    case ash::AppListConfigType::kMedium:
      return 4;
    case ash::AppListConfigType::kSmall:
      return 0;
  }

  NOTREACHED();
  return GridTitleHorizontalPaddingForType(ash::AppListConfigType::kShared);
}

int GridFocusDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 80;
    case ash::AppListConfigType::kMedium:
      return 64;
    case ash::AppListConfigType::kSmall:
      return 56;
  }

  NOTREACHED();
  return GridFocusDimensionForType(ash::AppListConfigType::kShared);
}

int GridFocusCornerRadiusForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 12;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
      return 8;
  }

  NOTREACHED();
  return GridFocusCornerRadiusForType(ash::AppListConfigType::kShared);
}

int AppTitleMaxLineHeightForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 20;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
      return 18;
  }

  NOTREACHED();
  return AppTitleMaxLineHeightForType(ash::AppListConfigType::kShared);
}

gfx::FontList AppTitleFontForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(1);
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
      return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(0);
  }

  NOTREACHED();
  return AppTitleFontForType(ash::AppListConfigType::kShared);
}

int FolderUnclippedIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 88;
    case ash::AppListConfigType::kMedium:
      return 64;
    case ash::AppListConfigType::kSmall:
      return 56;
  }

  NOTREACHED();
  return FolderUnclippedIconDimensionForType(ash::AppListConfigType::kShared);
}

int FolderClippedIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 72;
    case ash::AppListConfigType::kMedium:
      return 56;
    case ash::AppListConfigType::kSmall:
      return 48;
  }

  NOTREACHED();
  return FolderClippedIconDimensionForType(ash::AppListConfigType::kShared);
}

int ItemIconInFolderIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
      return 32;
    case ash::AppListConfigType::kMedium:
      return 28;
    case ash::AppListConfigType::kSmall:
      return 24;
  }

  NOTREACHED();
  return ItemIconInFolderIconDimensionForType(ash::AppListConfigType::kShared);
}

int ItemIconInFolderIconMarginForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kShared:
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kMedium:
      return 4;
    case ash::AppListConfigType::kSmall:
      return 2;
  }

  NOTREACHED();
  return ItemIconInFolderIconMarginForType(ash::AppListConfigType::kShared);
}

}  // namespace

AppListConfig::AppListConfig(ash::AppListConfigType type)
    : type_(type),
      scale_x_(1),
      scale_y_(1),
      grid_tile_width_(GridTileWidthForType(type)),
      grid_tile_height_(GridTileHeightForType(type)),
      grid_tile_spacing_(0),
      grid_icon_dimension_(GridIconDimensionForType(type)),
      grid_icon_bottom_padding_(24),
      grid_title_top_padding_(GridTitleTopPaddingForType(type)),
      grid_title_bottom_padding_(GridTitleBottomPaddingForType(type)),
      grid_title_horizontal_padding_(GridTitleHorizontalPaddingForType(type)),
      grid_title_width_(grid_tile_width_),
      grid_title_color_(SK_ColorWHITE),
      grid_focus_dimension_(GridFocusDimensionForType(type)),
      grid_focus_corner_radius_(GridFocusCornerRadiusForType(type)),
      grid_fadeout_zone_height_(24),
      search_tile_icon_dimension_(48),
      search_tile_badge_icon_dimension_(22),
      search_tile_badge_icon_offset_(5),
      search_list_icon_dimension_(20),
      search_list_icon_vertical_bar_dimension_(48),
      search_list_badge_icon_dimension_(14),
      suggestion_chip_icon_dimension_(16),
      app_title_max_line_height_(AppTitleMaxLineHeightForType(type)),
      app_title_font_(AppTitleFontForType(type)),
      peeking_app_list_height_(284),
      search_box_closed_top_padding_(0),
      search_box_peeking_top_padding_(84),
      search_box_fullscreen_top_padding_(24),
      search_box_preferred_size_for_dense_layout_(40),
      preferred_cols_(5),
      preferred_rows_(4),
      page_spacing_(48),
      expand_arrow_tile_height_(72),
      folder_bubble_radius_(FolderUnclippedIconDimensionForType(type) / 2),
      folder_bubble_y_offset_(0),
      folder_header_height_(20),
      folder_icon_dimension_(FolderClippedIconDimensionForType(type)),
      folder_unclipped_icon_dimension_(
          FolderUnclippedIconDimensionForType(type)),
      folder_icon_radius_(FolderClippedIconDimensionForType(type) / 2),
      folder_background_radius_(12),
      folder_bubble_color_(SkColorSetA(gfx::kGoogleGrey100, 0x7A)),
      item_icon_in_folder_icon_dimension_(
          ItemIconInFolderIconDimensionForType(type)),
      item_icon_in_folder_icon_margin_(ItemIconInFolderIconMarginForType(type)),
      folder_dropping_circle_radius_(folder_bubble_radius_),
      folder_dropping_delay_(0),
      folder_background_color_(gfx::kGoogleGrey100),
      page_flip_zone_size_(20),
      grid_tile_spacing_in_folder_(8),
      // TODO(manucornet): Share the value with ShelfConstants and use
      // 48 when the new shelf UI is turned off.
      shelf_height_(chromeos::switches::ShouldShowShelfHotseat() ? 48 : 56),
      background_radius_(shelf_height_ / 2),
      blur_radius_(30),
      contents_background_color_(SkColorSetRGB(0xF2, 0xF2, 0xF2)),
      grid_selected_color_(gfx::kGoogleBlue300),
      card_background_color_(SK_ColorWHITE),
      page_transition_duration_(base::TimeDelta::FromMilliseconds(250)),
      overscroll_page_transition_duration_(
          base::TimeDelta::FromMilliseconds(50)),
      folder_transition_in_duration_(base::TimeDelta::FromMilliseconds(250)),
      folder_transition_out_duration_(base::TimeDelta::FromMilliseconds(30)),
      num_start_page_tiles_(5),
      max_search_results_(6),
      max_folder_pages_(3),
      max_folder_items_per_page_(16),
      max_folder_name_chars_(28),
      all_apps_opacity_start_px_(8.0f),
      all_apps_opacity_end_px_(144.0f),
      search_result_title_font_style_(ui::ResourceBundle::BaseFont),
      search_tile_height_(92) {}

AppListConfig::AppListConfig(const AppListConfig& base_config,
                             float scale_x,
                             float scale_y,
                             float inner_tile_scale_y,
                             bool min_y_scale)
    : type_(base_config.type_),
      scale_x_(scale_x),
      scale_y_(scale_y),
      grid_tile_width_(MinScale(base_config.grid_tile_width_, scale_x, 1)),
      grid_tile_height_(
          MinScale(base_config.grid_tile_height_ -
                       (min_y_scale ? kMinYScaleHeightAdjustment : 0),
                   scale_y,
                   1)),
      grid_tile_spacing_(base_config.grid_tile_spacing_),
      grid_icon_dimension_(MinScale(base_config.grid_icon_dimension_,
                                    scale_x,
                                    inner_tile_scale_y)),
      grid_icon_bottom_padding_(
          MinScale(base_config.grid_icon_bottom_padding_ +
                       (min_y_scale ? kMinYScaleHeightAdjustment : 0),
                   inner_tile_scale_y,
                   1)),
      grid_title_top_padding_(
          MinScale(base_config.grid_title_top_padding_ -
                       (min_y_scale ? kMinYScaleHeightAdjustment : 0),
                   inner_tile_scale_y,
                   1)),
      grid_title_bottom_padding_(
          MinScale(base_config.grid_title_bottom_padding_,
                   inner_tile_scale_y,
                   1)),
      grid_title_horizontal_padding_(
          MinScale(base_config.grid_title_horizontal_padding_, scale_x, 1)),
      grid_title_width_(base_config.grid_tile_width_),
      grid_title_color_(base_config.grid_title_color_),
      grid_focus_dimension_(MinScale(base_config.grid_focus_dimension_,
                                     scale_x,
                                     inner_tile_scale_y)),
      grid_focus_corner_radius_(MinScale(base_config.grid_focus_corner_radius_,
                                         scale_x,
                                         inner_tile_scale_y)),
      grid_fadeout_zone_height_(
          min_y_scale
              ? 8
              : MinScale(base_config.grid_fadeout_zone_height_, scale_y, 1)),
      search_tile_icon_dimension_(base_config.search_tile_icon_dimension_),
      search_tile_badge_icon_dimension_(
          base_config.search_tile_badge_icon_dimension_),
      search_tile_badge_icon_offset_(
          base_config.search_tile_badge_icon_offset_),
      search_list_icon_dimension_(base_config.search_list_icon_dimension_),
      search_list_icon_vertical_bar_dimension_(
          base_config.search_list_icon_vertical_bar_dimension_),
      search_list_badge_icon_dimension_(
          base_config.search_list_badge_icon_dimension_),
      suggestion_chip_icon_dimension_(
          base_config.suggestion_chip_icon_dimension_),
      app_title_max_line_height_(base_config.app_title_max_line_height_),
      app_title_font_(base_config.app_title_font_.DeriveWithSizeDelta(
          min_y_scale ? -2 : (scale_y < 0.66 ? -1 : 0))),
      peeking_app_list_height_(base_config.peeking_app_list_height_),
      search_box_closed_top_padding_(
          base_config.search_box_closed_top_padding_),
      search_box_peeking_top_padding_(
          base_config.search_box_peeking_top_padding_),
      search_box_fullscreen_top_padding_(
          base_config.search_box_fullscreen_top_padding_),
      search_box_preferred_size_for_dense_layout_(
          base_config.search_box_preferred_size_for_dense_layout_),
      preferred_cols_(base_config.preferred_cols_),
      preferred_rows_(base_config.preferred_rows_),
      page_spacing_(base_config.page_spacing_),
      expand_arrow_tile_height_(base_config.expand_arrow_tile_height_),
      folder_bubble_radius_(MinScale(base_config.folder_bubble_radius_,
                                     scale_x,
                                     inner_tile_scale_y)),
      folder_bubble_y_offset_(base_config.folder_bubble_y_offset_),
      folder_header_height_(
          MinScale(base_config.folder_header_height_, scale_y, 1)),
      folder_icon_dimension_(MinScale(base_config.folder_icon_dimension_,
                                      scale_x,
                                      inner_tile_scale_y)),
      folder_unclipped_icon_dimension_(
          MinScale(base_config.folder_unclipped_icon_dimension_,
                   scale_x,
                   inner_tile_scale_y)),
      folder_icon_radius_(MinScale(base_config.folder_icon_radius_,
                                   scale_x,
                                   inner_tile_scale_y)),
      folder_background_radius_(
          MinScale(base_config.folder_background_radius_, scale_x, scale_y)),
      folder_bubble_color_(base_config.folder_bubble_color_),
      item_icon_in_folder_icon_dimension_(
          MinScale(base_config.item_icon_in_folder_icon_dimension_,
                   scale_x,
                   inner_tile_scale_y)),
      item_icon_in_folder_icon_margin_(
          MinScale(base_config.item_icon_in_folder_icon_margin_,
                   scale_x,
                   inner_tile_scale_y)),
      folder_dropping_circle_radius_(
          MinScale(base_config.folder_dropping_circle_radius_,
                   scale_x,
                   inner_tile_scale_y)),
      folder_dropping_delay_(base_config.folder_dropping_delay_),
      folder_background_color_(base_config.folder_background_color_),
      page_flip_zone_size_(base_config.page_flip_zone_size_),
      grid_tile_spacing_in_folder_(
          MinScale(base_config.grid_tile_spacing_in_folder_,
                   scale_x,
                   inner_tile_scale_y)),
      shelf_height_(base_config.shelf_height_),
      background_radius_(base_config.background_radius_),
      blur_radius_(base_config.blur_radius_),
      contents_background_color_(base_config.contents_background_color_),
      grid_selected_color_(base_config.grid_selected_color_),
      card_background_color_(base_config.card_background_color_),
      page_transition_duration_(base_config.page_transition_duration_),
      overscroll_page_transition_duration_(
          base_config.overscroll_page_transition_duration_),
      folder_transition_in_duration_(
          base_config.folder_transition_in_duration_),
      folder_transition_out_duration_(
          base_config.folder_transition_out_duration_),
      num_start_page_tiles_(base_config.num_start_page_tiles_),
      max_search_results_(base_config.max_search_results_),
      max_folder_pages_(base_config.max_folder_pages_),
      max_folder_items_per_page_(base_config.max_folder_items_per_page_),
      max_folder_name_chars_(base_config.max_folder_name_chars_),
      all_apps_opacity_start_px_(base_config.all_apps_opacity_start_px_),
      all_apps_opacity_end_px_(base_config.all_apps_opacity_end_px_),
      search_result_title_font_style_(
          base_config.search_result_title_font_style_),
      search_tile_height_(base_config.search_tile_height_) {
  DCHECK_EQ(type_, ash::AppListConfigType::kShared);
}

AppListConfig::~AppListConfig() = default;

// static
AppListConfig& AppListConfig::instance() {
  return *AppListConfigProvider::Get().GetConfigForType(
      ash::AppListConfigType::kShared, true /*can_create*/);
}

int AppListConfig::GetPreferredIconDimension(
    ash::SearchResultDisplayType display_type) const {
  switch (display_type) {
    case ash::SearchResultDisplayType::kRecommendation:
      FALLTHROUGH;
    case ash::SearchResultDisplayType::kTile:
      return search_tile_icon_dimension_;
    case ash::SearchResultDisplayType::kList:
      return search_list_icon_dimension_;
    case ash::SearchResultDisplayType::kNone:  // Falls through.
    case ash::SearchResultDisplayType::kCard:
      return 0;
    case ash::SearchResultDisplayType::kLast:
      break;
  }
  NOTREACHED();
  return 0;
}

int AppListConfig::GetMaxNumOfItemsPerPage(int /* page */) const {
  // In new style launcher, the first row of first page is no longger suggestion
  // apps.
  return preferred_cols_ * preferred_rows_;
}

}  // namespace app_list
