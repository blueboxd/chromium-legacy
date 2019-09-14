// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_CONFIG_H_
#define ASH_PUBLIC_CPP_SHELF_CONFIG_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// Provides layout and drawing config for the Shelf. Note That some of these
// values could change at runtime.
class ASH_EXPORT ShelfConfig : public TabletModeObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when shelf config values are changed.
    virtual void OnShelfConfigUpdated() {}
  };

  ShelfConfig();
  ~ShelfConfig() override;

  static ShelfConfig* Get();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Add observers to this objects's dependencies.
  void Init();

  // Remove observers from this object's dependencies.
  void Shutdown();

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Size of the shelf when visible (height when the shelf is horizontal and
  // width when the shelf is vertical).
  int shelf_size() const;

  // Size allocated for each app button on the shelf.
  int button_size() const;

  // Size of the space between buttons on the shelf.
  int button_spacing() const;

  // Size of the icons within shelf buttons.
  int button_icon_size() const;

  // Size for controls like the home button, back button, etc.
  int control_size() const;

  // The radius of shelf control buttons.
  int control_border_radius() const;

  // The margin around the overflow button on the shelf.
  int overflow_button_margin() const;

  // The distance between the edge of the shelf and the home and back button.
  int home_button_edge_spacing() const;

  // The extra padding added to status area tray buttons on the shelf.
  int status_area_hit_region_padding() const;

  int app_icon_group_margin() const { return app_icon_group_margin_; }
  SkColor shelf_control_permanent_highlight_background() const {
    return shelf_control_permanent_highlight_background_;
  }
  SkColor shelf_focus_border_color() const { return shelf_focus_border_color_; }
  int workspace_area_visible_inset() const {
    return workspace_area_visible_inset_;
  }
  int workspace_area_auto_hide_inset() const {
    return workspace_area_auto_hide_inset_;
  }
  int hidden_shelf_in_screen_portion() const {
    return hidden_shelf_in_screen_portion_;
  }
  SkColor shelf_default_base_color() const { return shelf_default_base_color_; }
  SkColor shelf_ink_drop_base_color() const {
    return shelf_ink_drop_base_color_;
  }
  float shelf_ink_drop_visible_opacity() const {
    return shelf_ink_drop_visible_opacity_;
  }
  SkColor shelf_icon_color() const { return shelf_icon_color_; }
  int shelf_translucent_over_app_list() const {
    return shelf_translucent_over_app_list_;
  }
  int shelf_translucent_alpha() const { return shelf_translucent_alpha_; }
  int shelf_translucent_maximized_window() const {
    return shelf_translucent_maximized_window_;
  }
  int shelf_translucent_color_darken_alpha() const {
    return shelf_translucent_color_darken_alpha_;
  }
  int shelf_opaque_color_darken_alpha() const {
    return shelf_opaque_color_darken_alpha_;
  }
  int status_indicator_offset_from_shelf_edge() const {
    return status_indicator_offset_from_shelf_edge_;
  }
  int shelf_tooltip_preview_height() const {
    return shelf_tooltip_preview_height_;
  }
  int shelf_tooltip_preview_max_width() const {
    return shelf_tooltip_preview_max_width_;
  }
  float shelf_tooltip_preview_max_ratio() const {
    return shelf_tooltip_preview_max_ratio_;
  }
  float shelf_tooltip_preview_min_ratio() const {
    return shelf_tooltip_preview_min_ratio_;
  }

 private:
  // Updates |is_dense_| and notifies all observers of the update.
  void UpdateIsDense();

  // Whether shelf is currently standard or dense.
  bool is_dense_;

  // Size of the shelf when visible (height when the shelf is horizontal and
  // width when the shelf is vertical).
  const int shelf_size_;
  const int shelf_size_dense_;

  // Size of the icons within shelf buttons.
  const int shelf_button_icon_size_;
  const int shelf_button_icon_size_dense_;

  // Size for controls like the home button, back button, etc.
  const int shelf_control_size_;
  const int shelf_control_size_dense_;

  // Size allocated for each app button on the shelf.
  const int shelf_button_size_;
  const int shelf_button_size_dense_;

  // Size of the space between buttons on the shelf.
  const int shelf_button_spacing_;

  // Size of the space between edge of screen and home button.
  const int shelf_home_button_edge_spacing_;
  const int shelf_home_button_edge_spacing_dense_;

  // The margin around the overflow button on the shelf.
  const int shelf_overflow_button_margin_;
  const int shelf_overflow_button_margin_dense_;

  // The extra padding added to status area tray buttons on the shelf.
  const int shelf_status_area_hit_region_padding_;
  const int shelf_status_area_hit_region_padding_dense_;

  // The margin on either side of the group of app icons (including the overflow
  // button).
  const int app_icon_group_margin_;

  const SkColor shelf_control_permanent_highlight_background_;

  const SkColor shelf_focus_border_color_;

  // We reserve a small area on the edge of the workspace area to ensure that
  // the resize handle at the edge of the window can be hit.
  const int workspace_area_visible_inset_;

  // When autohidden we extend the touch hit target onto the screen so that the
  // user can drag the shelf out.
  const int workspace_area_auto_hide_inset_;

  // Portion of the shelf that's within the screen bounds when auto-hidden.
  const int hidden_shelf_in_screen_portion_;

  // The default base color of the shelf to which different alpha values are
  // applied based on the desired shelf opacity level.
  const SkColor shelf_default_base_color_;

  // Ink drop color for shelf items.
  const SkColor shelf_ink_drop_base_color_;

  // Opacity of the ink drop ripple for shelf items when the ripple is visible.
  const float shelf_ink_drop_visible_opacity_;

  // The foreground color of the icons used in the shelf (launcher,
  // notifications, etc).
  const SkColor shelf_icon_color_;

  // The alpha value for the shelf background.
  const int shelf_translucent_over_app_list_;
  const int shelf_translucent_alpha_;
  // Using 0xFF causes clipping on the overlay candidate content, which prevent
  // HW overlay, probably due to a bug in compositor. Fix it and use 0xFF.
  // crbug.com/901538
  const int shelf_translucent_maximized_window_;

  // The alpha value used to darken a colorized shelf when the shelf is
  // translucent.
  const int shelf_translucent_color_darken_alpha_;

  // The alpha value used to darken a colorized shelf when the shelf is opaque.
  const int shelf_opaque_color_darken_alpha_;

  // The distance between the edge of the shelf and the status indicators.
  const int status_indicator_offset_from_shelf_edge_;

  // Dimensions for hover previews.
  const int shelf_tooltip_preview_height_;
  const int shelf_tooltip_preview_max_width_;
  const float shelf_tooltip_preview_max_ratio_;
  const float shelf_tooltip_preview_min_ratio_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfConfig);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_CONFIG_H_
