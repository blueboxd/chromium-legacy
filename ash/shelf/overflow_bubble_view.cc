// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble_view.h"

#include <algorithm>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
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
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Padding at the two ends of the shelf in overflow mode.
constexpr int kEndPadding = 4;

// Padding between the end of the shelf in overview mode and the arrow button
// (if any).
constexpr int kDistanceToArrowButton = kShelfButtonSpacing;

// Distance between overflow bubble and the main shelf.
constexpr int kDistanceToMainShelf = 4;

// Minimum margin around the bubble so that it doesn't hug the screen edges.
constexpr int kMinimumMargin = 8;

// Size of the arrow button.
constexpr int kArrowButtonSize = kShelfControlSize;

// Sum of the shelf button size and the gap between shelf buttons.
constexpr int kUnit = kShelfButtonSize + kShelfButtonSpacing;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ScrollArrowView

OverflowBubbleView::ScrollArrowView::ScrollArrowView(
    ArrowType arrow_type,
    bool is_horizontal_alignment)
    : arrow_type_(arrow_type),
      is_horizontal_alignment_(is_horizontal_alignment) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

void OverflowBubbleView::ScrollArrowView::OnPaint(gfx::Canvas* canvas) {
  gfx::ImageSkia img = CreateVectorIcon(
      arrow_type_ == LEFT ? kOverflowShelfLeftIcon : kOverflowShelfRightIcon,
      SK_ColorWHITE);

  if (!is_horizontal_alignment_) {
    img = gfx::ImageSkiaOperations::CreateRotatedImage(
        img, SkBitmapOperations::ROTATION_90_CW);
  }

  gfx::PointF center_point(width() / 2.f, height() / 2.f);
  canvas->DrawImageInt(img, center_point.x() - img.width() / 2,
                       center_point.y() - img.height() / 2);

  float ring_radius_dp = kArrowButtonSize / 2;
  {
    gfx::PointF circle_center(center_point);
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    circle_center.Scale(dsf);
    cc::PaintFlags fg_flags;
    fg_flags.setAntiAlias(true);
    fg_flags.setColor(kShelfControlPermanentHighlightBackground);

    const float radius = std::ceil(ring_radius_dp * dsf);
    canvas->DrawCircle(circle_center, radius, fg_flags);
  }
}

const char* OverflowBubbleView::ScrollArrowView::GetClassName() const {
  return "ScrollArrowView";
}

////////////////////////////////////////////////////////////////////////////////
// OverflowShelfContainerView impl

OverflowBubbleView::OverflowShelfContainerView::OverflowShelfContainerView(
    ShelfView* shelf_view)
    : shelf_view_(shelf_view) {}

void OverflowBubbleView::OverflowShelfContainerView::Initialize() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  shelf_view_->SetPaintToLayer();
  shelf_view_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(shelf_view_);
}

gfx::Size
OverflowBubbleView::OverflowShelfContainerView::CalculatePreferredSize() const {
  return shelf_view_->CalculatePreferredSize();
}

void OverflowBubbleView::OverflowShelfContainerView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void OverflowBubbleView::OverflowShelfContainerView::Layout() {
  shelf_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(), shelf_view_->GetPreferredSize()));
}

const char* OverflowBubbleView::OverflowShelfContainerView::GetClassName()
    const {
  return "OverflowShelfContainerView";
}

void OverflowBubbleView::OverflowShelfContainerView::TranslateShelfView(
    const gfx::Vector2d& offset) {
  gfx::Transform transform_matrix;
  transform_matrix.Translate(-offset);
  shelf_view_->SetTransform(transform_matrix);
}

////////////////////////////////////////////////////////////////////////////////
// OverflowBubbleView

OverflowBubbleView::OverflowBubbleView(ShelfView* shelf_view,
                                       views::View* anchor,
                                       SkColor background_color)
    : ShelfBubble(anchor, shelf_view->shelf()->alignment(), background_color),
      shelf_(shelf_view->shelf()) {
  DCHECK(shelf_);

  set_border_radius(ShelfConstants::shelf_size() / 2);
  SetArrow(views::BubbleBorder::NONE);
  SetBackground(nullptr);
  set_shadow(views::BubbleBorder::NO_ASSETS);
  set_close_on_deactivate(false);
  set_accept_events(true);
  set_margins(gfx::Insets(0, 0));

  // Makes bubble view has a layer and clip its children layers.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  // Initialize the left arrow button.
  left_arrow_ = new ScrollArrowView(ScrollArrowView::LEFT,
                                    shelf_->IsHorizontalAlignment());
  AddChildView(left_arrow_);

  // Initialize the right arrow button.
  right_arrow_ = new ScrollArrowView(ScrollArrowView::RIGHT,
                                     shelf_->IsHorizontalAlignment());
  AddChildView(right_arrow_);

  // Initialize the shelf container view.
  shelf_container_view_ = new OverflowShelfContainerView(shelf_view);
  shelf_container_view_->Initialize();
  AddChildView(shelf_container_view_);

  CreateBubble();
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
  const int old_x = scroll_offset_.x();
  const int x = CalculateLayoutStrategyAfterScroll(x_offset);
  scroll_offset_.set_x(x);
  Layout();
  return x - old_x;
}

int OverflowBubbleView::ScrollByYOffset(int y_offset) {
  const int old_y = scroll_offset_.y();
  const int y = CalculateLayoutStrategyAfterScroll(y_offset);
  scroll_offset_.set_y(y);
  Layout();
  return y - old_y;
}

int OverflowBubbleView::CalculateScrollUpperBound() const {
  const bool is_horizontal = shelf_->IsHorizontalAlignment();

  // Calculate the length of the available space.
  const gfx::Rect content_size = GetContentsBounds();
  const int available_length =
      (is_horizontal ? content_size.width() : content_size.height()) -
      2 * kEndPadding;

  // Calculate the length of the preferred size.
  const gfx::Size shelf_preferred_size(
      shelf_container_view_->GetPreferredSize());
  const int preferred_length = (is_horizontal ? shelf_preferred_size.width()
                                              : shelf_preferred_size.height());

  DCHECK_GE(preferred_length, available_length);
  return preferred_length - available_length;
}

int OverflowBubbleView::CalculateLayoutStrategyAfterScroll(int scroll) {
  const int old_scroll =
      shelf_->IsHorizontalAlignment() ? scroll_offset_.x() : scroll_offset_.y();

  const int scroll_upper_bound = CalculateScrollUpperBound();
  scroll = std::min(scroll_upper_bound, std::max(0, old_scroll + scroll));
  if (layout_strategy_ != NOT_SHOW_ARROW_BUTTON) {
    if (scroll <= 0)
      layout_strategy_ = SHOW_RIGHT_ARROW_BUTTON;
    else if (scroll >= scroll_upper_bound)
      layout_strategy_ = SHOW_LEFT_ARROW_BUTTON;
    else
      layout_strategy_ = SHOW_BUTTONS;
  }
  return scroll;
}

void OverflowBubbleView::AdjustToEnsureIconsFullyVisible(
    gfx::Rect* bubble_bounds) const {
  if (layout_strategy_ == NOT_SHOW_ARROW_BUTTON)
    return;

  int width = shelf_->IsHorizontalAlignment() ? bubble_bounds->width()
                                              : bubble_bounds->height();
  const int rd = width % kUnit;
  width -= rd;

  // Offset to ensure that the bubble view is shown at the center of the screen.
  if (shelf_->IsHorizontalAlignment()) {
    bubble_bounds->set_width(width);
    bubble_bounds->Offset(rd / 2, 0);
  } else {
    bubble_bounds->set_height(width);
    bubble_bounds->Offset(0, rd / 2);
  }
}

gfx::Size OverflowBubbleView::CalculatePreferredSize() const {
  gfx::Rect monitor_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(GetAnchorRect().CenterPoint())
          .work_area();
  monitor_rect.Inset(gfx::Insets(kMinimumMargin));
  int available_length = shelf_->IsHorizontalAlignment()
                             ? monitor_rect.width()
                             : monitor_rect.height();

  gfx::Size preferred_size = shelf_container_view_->GetPreferredSize();
  int preferred_length = shelf_->IsHorizontalAlignment()
                             ? preferred_size.width()
                             : preferred_size.height();
  preferred_length += 2 * kEndPadding;
  int scroll_length =
      shelf_->IsHorizontalAlignment() ? scroll_offset_.x() : scroll_offset_.y();

  if (preferred_length <= available_length) {
    // Enough space to accommodate all of shelf buttons. So hide arrow buttons.
    layout_strategy_ = NOT_SHOW_ARROW_BUTTON;
  } else if (scroll_length == 0) {
    // No invisible shelf buttons at the left side. So hide the left button.
    layout_strategy_ = SHOW_RIGHT_ARROW_BUTTON;
  } else if (scroll_length == CalculateScrollUpperBound()) {
    // If there is no invisible shelf button at the right side, hide the right
    // button.
    layout_strategy_ = SHOW_LEFT_ARROW_BUTTON;
  } else {
    // There are invisible shelf buttons at both sides. So show two buttons.
    layout_strategy_ = SHOW_BUTTONS;
  }

  if (shelf_->IsHorizontalAlignment()) {
    preferred_size.set_width(std::min(preferred_length, monitor_rect.width()));
  } else {
    preferred_size.set_height(
        std::min(preferred_length, monitor_rect.height()));
  }
  return preferred_size;
}

void OverflowBubbleView::Layout() {
  constexpr gfx::Size shelf_button_size(kShelfButtonSize, kShelfButtonSize);
  constexpr gfx::Size arrow_button_size(kArrowButtonSize, kArrowButtonSize);

  bool is_horizontal = shelf_->IsHorizontalAlignment();
  gfx::Rect shelf_container_bounds = bounds();

  // Transpose and layout as if it is horizontal.
  if (!is_horizontal)
    shelf_container_bounds.Transpose();

  // The bounds of |left_arrow_| and |right_arrow_| in the parent coordinates.
  gfx::Rect left_arrow_bounds, right_arrow_bounds;

  // Calculates the bounds of the left arrow button. If the left arrow button
  // should not show, |left_arrow_bounds| should be empty.
  if (layout_strategy_ == SHOW_LEFT_ARROW_BUTTON ||
      layout_strategy_ == SHOW_BUTTONS) {
    left_arrow_bounds = gfx::Rect(shelf_button_size);
    left_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(kShelfButtonSize + kDistanceToArrowButton, 0,
                                 0, 0);
  }

  if (layout_strategy_ == SHOW_RIGHT_ARROW_BUTTON ||
      layout_strategy_ == SHOW_BUTTONS) {
    shelf_container_bounds.Inset(0, 0, kShelfButtonSize, 0);
    right_arrow_bounds =
        gfx::Rect(shelf_container_bounds.top_right(), shelf_button_size);
    right_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(0, 0, kDistanceToArrowButton, 0);
  }

  shelf_container_bounds.Inset(kEndPadding, 0, kEndPadding, 0);

  // Adjust the bounds when not showing in the horizontal alignment.
  if (!shelf_->IsHorizontalAlignment()) {
    left_arrow_bounds.Transpose();
    right_arrow_bounds.Transpose();
    shelf_container_bounds.Transpose();
  }

  // Draw |left_arrow| if it should show.
  left_arrow_->SetVisible(!left_arrow_bounds.IsEmpty());
  if (left_arrow_->GetVisible())
    left_arrow_->SetBoundsRect(left_arrow_bounds);

  // Draw |right_arrow| if it should show.
  right_arrow_->SetVisible(!right_arrow_bounds.IsEmpty());
  if (right_arrow_->GetVisible())
    right_arrow_->SetBoundsRect(right_arrow_bounds);

  // Draw |shelf_container_view_|.
  shelf_container_view_->SetBoundsRect(shelf_container_bounds);

  // When the left button shows, the origin of |shelf_container_view_| changes.
  // So translate |shelf_container_view| to show the shelf view correctly.
  gfx::Vector2d translate_vector;
  if (!left_arrow_bounds.IsEmpty()) {
    translate_vector = shelf_->IsHorizontalAlignment()
                           ? gfx::Vector2d(shelf_container_bounds.x(), 0)
                           : gfx::Vector2d(0, shelf_container_bounds.y());
  }

  shelf_container_view_->TranslateShelfView(scroll_offset_ + translate_vector);
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

  gfx::Rect bounds;
  if (shelf_->IsHorizontalAlignment()) {
    bounds = gfx::Rect(
        base::i18n::IsRTL() ? anchor_rect.x()
                            : anchor_rect.right() - content_size.width(),
        anchor_rect.y() - distance_to_overflow_button - content_size.height(),
        content_size.width(), content_size.height());
    if (bounds.x() < monitor_rect.x())
      bounds.Offset(monitor_rect.x() - bounds.x(), 0);
    if (bounds.right() > monitor_rect.right())
      bounds.set_width(monitor_rect.right() - bounds.x());
  } else {
    bounds = gfx::Rect(0, anchor_rect.bottom() - content_size.height(),
                       content_size.width(), content_size.height());
    if (shelf_->alignment() == SHELF_ALIGNMENT_LEFT)
      bounds.set_x(anchor_rect.right() + distance_to_overflow_button);
    else
      bounds.set_x(anchor_rect.x() - distance_to_overflow_button -
                   content_size.width());
    if (bounds.y() < monitor_rect.y())
      bounds.Offset(0, monitor_rect.y() - bounds.y());
    if (bounds.bottom() > monitor_rect.bottom())
      bounds.set_height(monitor_rect.bottom() - bounds.y());
  }

  AdjustToEnsureIconsFullyVisible(&bounds);
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
