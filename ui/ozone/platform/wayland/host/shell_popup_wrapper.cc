// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

namespace ui {

constexpr uint32_t kAnchorDefaultWidth = 1;
constexpr uint32_t kAnchorDefaultHeight = 1;
constexpr uint32_t kAnchorHeightParentMenu = 30;

gfx::Rect GetAnchorRect(MenuType menu_type,
                        const gfx::Rect& menu_bounds,
                        const gfx::Rect& parent_window_bounds) {
  gfx::Rect anchor_rect;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      // Place anchor for right click menus normally.
      anchor_rect = gfx::Rect(menu_bounds.x(), menu_bounds.y(),
                              kAnchorDefaultWidth, kAnchorDefaultHeight);
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      // The anchor for parent menu windows is positioned slightly above the
      // specified bounds to ensure flipped window along y-axis won't hide 3-dot
      // menu button.
      anchor_rect = gfx::Rect(menu_bounds.x() - kAnchorDefaultWidth,
                              menu_bounds.y() - kAnchorHeightParentMenu,
                              kAnchorDefaultWidth, kAnchorHeightParentMenu);
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      // The child menu's anchor must meet the following requirements: at some
      // point, the Wayland compositor can flip it along x-axis. To make sure
      // it's positioned correctly, place it closer to the beginning of the
      // parent menu shifted by the same value along x-axis. The width of anchor
      // must correspond the width between two points - specified origin by the
      // Chromium and calculated point shifted by the same value along x-axis
      // from the beginning of the parent menu width.
      //
      // We also have to bear in mind that Chromium may decide to flip the
      // position of the menu window along the x-axis and show it on the other
      // side of the parent menu window (normally, the Wayland compositor does
      // it). Thus, check which side the child menu window is going to be
      // presented on and create right anchor.
      if (menu_bounds.x() >= 0) {
        auto anchor_width =
            parent_window_bounds.width() -
            (parent_window_bounds.width() - menu_bounds.x()) * 2;
        if (anchor_width <= 0) {
          anchor_rect = gfx::Rect(menu_bounds.x(), menu_bounds.y(),
                                  kAnchorDefaultWidth, kAnchorDefaultHeight);
        } else {
          anchor_rect =
              gfx::Rect(parent_window_bounds.width() - menu_bounds.x(),
                        menu_bounds.y(), anchor_width, kAnchorDefaultHeight);
        }
      } else {
        DCHECK_LE(menu_bounds.x(), 0);
        auto position = menu_bounds.width() + menu_bounds.x();
        DCHECK(position > 0 && position < parent_window_bounds.width());
        auto anchor_width = parent_window_bounds.width() - position * 2;
        if (anchor_width <= 0) {
          anchor_rect = gfx::Rect(position, menu_bounds.y(),
                                  kAnchorDefaultWidth, kAnchorDefaultHeight);
        } else {
          anchor_rect = gfx::Rect(position, menu_bounds.y(), anchor_width,
                                  kAnchorDefaultHeight);
        }
      }
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return anchor_rect;
}

}  // namespace ui
