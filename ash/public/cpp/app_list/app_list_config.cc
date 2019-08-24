// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/gfx/color_palette.h"

namespace app_list {

namespace {

// The minimum scale that can be used when scaling down the app list view UI.
// Selected so the grid tile height does not get smaller than 80 dip.
constexpr float kMinimumConfigScale = 80. / 120.;

// Scales |value| using the smaller one of |scale_1| and |scale_2|.
int MinScale(int value, float scale_1, float scale_2) {
  return std::round(value * std::min(scale_1, scale_2));
}

}  // namespace

AppListConfig::AppListConfig() : AppListConfig(1, 1, 1) {}

AppListConfig::AppListConfig(float scale_x,
                             float scale_y,
                             float inner_tile_scale_y)
    : scale_x_(scale_x),
      scale_y_(scale_y),
      grid_tile_width_(MinScale(112, scale_x, 1)),
      grid_tile_height_(MinScale(120, scale_y, 1)),
      grid_tile_spacing_(0),
      grid_icon_dimension_(MinScale(64, scale_x, inner_tile_scale_y)),
      grid_icon_bottom_padding_(MinScale(24, inner_tile_scale_y, 1)),
      grid_title_top_padding_(MinScale(92, inner_tile_scale_y, 1)),
      grid_title_bottom_padding_(MinScale(8, inner_tile_scale_y, 1)),
      grid_title_horizontal_padding_(MinScale(8, scale_x, 1)),
      grid_title_width_(grid_tile_width_),
      grid_title_color_(SK_ColorWHITE),
      grid_focus_dimension_(MinScale(80, scale_x, inner_tile_scale_y)),
      grid_focus_corner_radius_(MinScale(12, scale_x, inner_tile_scale_y)),
      search_tile_icon_dimension_(48),
      search_tile_badge_icon_dimension_(22),
      search_tile_badge_icon_offset_(5),
      search_list_icon_dimension_(20),
      search_list_icon_vertical_bar_dimension_(48),
      search_list_badge_icon_dimension_(14),
      suggestion_chip_icon_dimension_(16),
      app_title_max_line_height_(20),
      app_title_font_(
          ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(1)),
      peeking_app_list_height_(284),
      search_box_closed_top_padding_(0),
      search_box_peeking_top_padding_(84),
      search_box_fullscreen_top_padding_(24),
      preferred_cols_(5),
      preferred_rows_(4),
      page_spacing_(48),
      expand_arrow_tile_height_(72),
      folder_bubble_radius_(MinScale(44, scale_x, inner_tile_scale_y)),
      folder_bubble_y_offset_(0),
      folder_header_height_(MinScale(20, scale_y, 1)),
      folder_icon_dimension_(MinScale(72, scale_x, inner_tile_scale_y)),
      folder_unclipped_icon_dimension_(
          MinScale(88, scale_x, inner_tile_scale_y)),
      folder_icon_radius_(MinScale(36, scale_x, inner_tile_scale_y)),
      folder_background_radius_(MinScale(12, scale_x, scale_y)),
      folder_bubble_color_(SkColorSetA(gfx::kGoogleGrey100, 0x7A)),
      item_icon_in_folder_icon_dimension_(
          MinScale(32, scale_x, inner_tile_scale_y)),
      folder_dropping_circle_radius_(MinScale(44, scale_x, scale_y)),
      folder_dropping_delay_(0),
      folder_background_color_(gfx::kGoogleGrey100),
      page_flip_zone_size_(20),
      grid_tile_spacing_in_folder_(MinScale(8, scale_x, inner_tile_scale_y)),
      // TODO(manucornet): Share the value with ShelfConstants and use
      // 48 when the new shelf UI is turned off.
      shelf_height_(chromeos::switches::ShouldShowShelfDenseClamshell() ? 48
                                                                        : 56),
      background_radius_(shelf_height_ / 2),
      blur_radius_(30),
      contents_background_color_(SkColorSetRGB(0xF2, 0xF2, 0xF2)),
      grid_selected_color_(gfx::kGoogleBlue300),
      card_background_color_(SK_ColorWHITE),
      page_transition_duration_ms_(250),
      overscroll_page_transition_duration_ms_(50),
      folder_transition_in_duration_ms_(250),
      folder_transition_out_duration_ms_(30),
      num_start_page_tiles_(5),
      max_search_results_(6),
      max_folder_pages_(3),
      max_folder_items_per_page_(16),
      max_folder_name_chars_(28),
      all_apps_opacity_start_px_(8.0f),
      all_apps_opacity_end_px_(144.0f),
      search_result_title_font_style_(ui::ResourceBundle::BaseFont),
      search_tile_height_(92) {}

AppListConfig::~AppListConfig() = default;

// static
AppListConfig& AppListConfig::instance() {
  static base::NoDestructor<AppListConfig> instance;
  return *instance;
}

std::unique_ptr<AppListConfig> AppListConfig::CreateForAppListWidget(
    const gfx::Size& display_work_area_size,
    const gfx::Size& available_grid_size,
    const AppListConfig* current_config) {
  DCHECK_EQ(this, &instance());

  const int min_grid_height =
      (display_work_area_size.width() < display_work_area_size.height()
           ? preferred_cols_
           : preferred_rows_) *
      grid_tile_height_;
  const int min_grid_width =
      (display_work_area_size.width() < display_work_area_size.height()
           ? preferred_rows_
           : preferred_cols_) *
      grid_tile_width_;

  float scale_x = 1;
  float scale_y = 1;
  float inner_tile_scale_y = 1;

  if (available_grid_size.height() < min_grid_height) {
    scale_y = std::max(
        kMinimumConfigScale,
        static_cast<float>(available_grid_size.height()) / min_grid_height);
    // Adjust scale to reflect the fact the app list item title height does not
    // get scaled. The adjustment is derived from:
    // s * x + c = S * (x + c) and t = x + c
    // With: S - the target grid scale,
    //       x - scalable part of the tile (total title padding),
    //       c - constant part of the tile,
    //       t - tile height, and
    //       s - the adjusted scale.
    const int total_title_padding =
        grid_title_bottom_padding_ + grid_title_top_padding_;
    inner_tile_scale_y =
        (grid_tile_height_ * (scale_y - 1) + total_title_padding) /
        total_title_padding;
  }

  if (available_grid_size.width() < min_grid_width) {
    scale_x = std::max(
        kMinimumConfigScale,
        static_cast<float>(available_grid_size.width()) / min_grid_width);
  }

  if (current_config && current_config->scale_x() == scale_x &&
      current_config->scale_y() == scale_y) {
    return nullptr;
  }

  return std::make_unique<AppListConfig>(scale_x, scale_y, inner_tile_scale_y);
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
