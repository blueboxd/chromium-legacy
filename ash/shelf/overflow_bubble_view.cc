// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble_view.h"

#include <algorithm>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/wm/window_util.h"
#include "base/i18n/rtl.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Padding at the two ends of the bubble.
constexpr int kEndPadding = 16;

// Distance between overflow bubble and the main shelf.
constexpr int kDistanceToMainShelf = 4;

// Minimum margin around the bubble so that it doesn't hug the screen edges.
constexpr int kMinimumMargin = 8;

}  // namespace

OverflowBubbleView::OverflowBubbleView(ShelfView* shelf_view,
                                       views::View* anchor,
                                       SkColor background_color)
    : ShelfBubble(anchor, shelf_view->shelf()->alignment(), background_color),
      shelf_(shelf_view->shelf()),
      shelf_view_(shelf_view) {
  DCHECK(shelf_);

  set_border_radius(ShelfConstants::shelf_size() / 2);
  SetArrow(views::BubbleBorder::NONE);
  SetBackground(nullptr);
  set_shadow(views::BubbleBorder::NO_ASSETS);
  set_close_on_deactivate(false);
  set_accept_events(true);

  if (shelf_->IsHorizontalAlignment())
    set_margins(gfx::Insets(0, kEndPadding));
  else
    set_margins(gfx::Insets(kEndPadding, 0));

  // Makes bubble view has a layer and clip its children layers.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  CreateBubble();

  AddChildView(shelf_view_);
}

OverflowBubbleView::~OverflowBubbleView() = default;

bool OverflowBubbleView::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Handle scroll-related events, but don't do anything special for begin and
  // end.
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event.type() == ui::ET_GESTURE_SCROLL_END) {
    return true;
  }
  if (event.type() != ui::ET_GESTURE_SCROLL_UPDATE)
    return false;

  if (shelf_->IsHorizontalAlignment())
    ScrollByXOffset(static_cast<int>(-event.details().scroll_x()));
  else
    ScrollByYOffset(static_cast<int>(-event.details().scroll_y()));
  return true;
}

int OverflowBubbleView::ScrollByXOffset(int x_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(shelf_view_->GetPreferredSize());

  DCHECK_GE(contents_size.width(), visible_bounds.width());
  const int old_x = scroll_offset_.x();
  const int x = std::min(contents_size.width() - visible_bounds.width(),
                         std::max(0, old_x + x_offset));
  scroll_offset_.set_x(x);
  Layout();
  return x - old_x;
}

int OverflowBubbleView::ScrollByYOffset(int y_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(shelf_view_->GetPreferredSize());

  DCHECK_GE(contents_size.height(), visible_bounds.height());
  const int old_y = scroll_offset_.y();
  const int y = std::min(contents_size.height() - visible_bounds.height(),
                         std::max(0, old_y + y_offset));
  scroll_offset_.set_y(y);
  Layout();
  return y - old_y;
}

gfx::Size OverflowBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = shelf_view_->GetPreferredSize();

  const gfx::Rect monitor_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(GetAnchorRect().CenterPoint())
          .work_area();
  if (!monitor_rect.IsEmpty()) {
    if (shelf_->IsHorizontalAlignment()) {
      preferred_size.set_width(std::min(
          preferred_size.width(), monitor_rect.width() - 2 * kEndPadding));
    } else {
      preferred_size.set_height(std::min(
          preferred_size.height(), monitor_rect.height() - 2 * kEndPadding));
    }
  }

  return preferred_size;
}

void OverflowBubbleView::Layout() {
  shelf_view_->SetBoundsRect(
      gfx::Rect(gfx::PointAtOffsetFromOrigin(-scroll_offset_),
                shelf_view_->GetPreferredSize()));
}

void OverflowBubbleView::ChildPreferredSizeChanged(views::View* child) {
  // When contents size is changed, ContentsBounds should be updated before
  // calculating scroll offset.
  SizeToContents();

  // Ensures |shelf_view_| is still visible.
  if (shelf_->IsHorizontalAlignment())
    ScrollByXOffset(0);
  else
    ScrollByYOffset(0);
}

bool OverflowBubbleView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // The MouseWheelEvent was changed to support both X and Y offsets
  // recently, but the behavior of this function was retained to continue
  // using Y offsets only. Might be good to simply scroll in both
  // directions as in OverflowBubbleView::OnScrollEvent.
  if (shelf_->IsHorizontalAlignment())
    ScrollByXOffset(-event.y_offset());
  else
    ScrollByYOffset(-event.y_offset());

  return true;
}

const char* OverflowBubbleView::GetClassName() const {
  return "OverflowBubbleView";
}

void OverflowBubbleView::OnScrollEvent(ui::ScrollEvent* event) {
  if (shelf_->IsHorizontalAlignment())
    ScrollByXOffset(static_cast<int>(-event->x_offset()));
  else
    ScrollByYOffset(static_cast<int>(-event->y_offset()));
  event->SetHandled();
}

gfx::Rect OverflowBubbleView::GetBubbleBounds() {
  const gfx::Size content_size = GetPreferredSize();
  const gfx::Rect anchor_rect = GetAnchorRect();
  const int distance_to_overflow_button =
      kDistanceToMainShelf +
      (ShelfConstants::shelf_size() - ShelfConstants::control_size()) / 2;
  gfx::Rect monitor_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  // Make sure no part of the bubble touches a screen edge.
  monitor_rect.Inset(gfx::Insets(kMinimumMargin));

  if (shelf_->IsHorizontalAlignment()) {
    gfx::Rect bounds(
        base::i18n::IsRTL()
            ? anchor_rect.x() - kEndPadding
            : anchor_rect.right() - content_size.width() - kEndPadding,
        anchor_rect.y() - distance_to_overflow_button - content_size.height(),
        content_size.width() + 2 * kEndPadding, content_size.height());
    if (bounds.x() < monitor_rect.x())
      bounds.Offset(monitor_rect.x() - bounds.x(), 0);
    if (bounds.right() > monitor_rect.right())
      bounds.set_width(monitor_rect.right() - bounds.x());
    return bounds;
  }
  gfx::Rect bounds(
      0, anchor_rect.bottom() - content_size.height() - kEndPadding,
      content_size.width(), content_size.height() + 2 * kEndPadding);
  if (shelf_->alignment() == SHELF_ALIGNMENT_LEFT)
    bounds.set_x(anchor_rect.right() + distance_to_overflow_button);
  else
    bounds.set_x(anchor_rect.x() - distance_to_overflow_button -
                 content_size.width());
  if (bounds.y() < monitor_rect.y())
    bounds.Offset(0, monitor_rect.y() - bounds.y());
  if (bounds.bottom() > monitor_rect.bottom())
    bounds.set_height(monitor_rect.bottom() - bounds.y());
  return bounds;
}

bool OverflowBubbleView::CanActivate() const {
  if (!GetWidget())
    return false;

  // Do not activate the bubble unless the current active window is the shelf
  // window.
  aura::Window* active_window = wm::GetActiveWindow();
  aura::Window* bubble_window = GetWidget()->GetNativeWindow();
  aura::Window* shelf_window = shelf_->shelf_widget()->GetNativeWindow();
  return active_window == bubble_window || active_window == shelf_window;
}

bool OverflowBubbleView::ShouldCloseOnPressDown() {
  return false;
}

bool OverflowBubbleView::ShouldCloseOnMouseExit() {
  return false;
}

}  // namespace ash
