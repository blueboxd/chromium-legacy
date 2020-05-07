// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_delegate.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

// The following distances are copied from tablet_mode_window_drag_delegate.cc.
// TODO(https://crbug.com/1069869): share these constants.

// Items dragged to within |kDistanceFromEdgeDp| of the screen will get snapped
// even if they have not moved by |kMinimumDragToSnapDistanceDp|.
constexpr float kDistanceFromEdgeDp = 16.f;
// The minimum distance that an item must be moved before it is snapped. This
// prevents accidental snaps.
constexpr float kMinimumDragToSnapDistanceDp = 96.f;

}  // namespace

// static
bool TabDragDropDelegate::IsChromeTabDrag(const ui::OSExchangeData& drag_data) {
  if (!features::IsWebUITabStripTabDragIntegrationEnabled())
    return false;

  base::Pickle pickle;
  drag_data.GetPickledData(ui::ClipboardFormatType::GetWebCustomDataType(),
                           &pickle);
  base::PickleIterator iter(pickle);

  uint32_t entry_count = 0;
  if (!iter.ReadUInt32(&entry_count))
    return false;

  for (uint32_t i = 0; i < entry_count; ++i) {
    base::StringPiece16 type;
    base::StringPiece16 data;
    if (!iter.ReadStringPiece16(&type) || !iter.ReadStringPiece16(&data))
      return false;

    // TODO(https://crbug.com/1069869): share this constant between Ash
    // and Chrome instead of hardcoding it in both places.
    static const base::NoDestructor<base::string16> chrome_tab_type(
        base::ASCIIToUTF16("application/vnd.chromium.tab"));
    if (type == *chrome_tab_type)
      return true;
  }

  return false;
}

TabDragDropDelegate::TabDragDropDelegate(
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& start_location_in_screen)
    : root_window_(root_window),
      source_window_(source_window),
      start_location_in_screen_(start_location_in_screen) {
  split_view_drag_indicators_ =
      std::make_unique<SplitViewDragIndicators>(root_window_);
}

TabDragDropDelegate::~TabDragDropDelegate() = default;

void TabDragDropDelegate::DragUpdate(const gfx::Point& location_in_screen) {
  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  SplitViewController::SnapPosition snap_position =
      ::ash::GetSnapPositionForLocation(
          Shell::GetPrimaryRootWindow(), location_in_screen,
          start_location_in_screen_,
          /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
          /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
          /*horizontal_edge_inset=*/area.width() *
                  kHighlightScreenPrimaryAxisRatio +
              kHighlightScreenEdgePaddingDp,
          /*vertical_edge_inset=*/area.height() *
                  kHighlightScreenPrimaryAxisRatio +
              kHighlightScreenEdgePaddingDp);
  split_view_drag_indicators_->SetWindowDraggingState(
      SplitViewDragIndicators::ComputeWindowDraggingState(
          true, SplitViewDragIndicators::WindowDraggingState::kFromTop,
          snap_position));

  // TODO(https://crbug.com/1069869): scale source window up/down similar to
  // |TabletModeBrowserWindowDragDelegate::UpdateSourceWindow()|.
}

void TabDragDropDelegate::Drop(const gfx::Point& location_in_screen,
                               const ui::OSExchangeData& drop_data) {
  aura::Window* const new_window =
      Shell::Get()->shell_delegate()->CreateBrowserForTabDrop(source_window_,
                                                              drop_data);

  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  SplitViewController::SnapPosition snap_position = ash::GetSnapPosition(
      root_window_, new_window, location_in_screen, start_location_in_screen_,
      /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
      /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
      /*horizontal_edge_inset=*/area.width() *
              kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp,
      /*vertical_edge_inset=*/area.height() * kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp);

  if (snap_position == SplitViewController::SnapPosition::NONE)
    return;

  SplitViewController::Get(new_window)->SnapWindow(new_window, snap_position);
}

}  // namespace ash
