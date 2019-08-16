// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

namespace {

// Padding between the the shelf container view and the arrow button (if any).
int GetDistanceToArrowButton() {
  return ShelfConstants::button_spacing();
}

// Sum of the shelf button size and the gap between shelf buttons.
int GetUnit() {
  return ShelfConstants::button_size() + ShelfConstants::button_spacing();
}

// Decides whether the current first visible shelf icon of the scrollable shelf
// should be hidden or fully shown when gesture scroll ends.
int GetGestureDragThreshold() {
  return ShelfConstants::button_size() / 2;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfView

ScrollableShelfView::ScrollableShelfView(ShelfModel* model, Shelf* shelf)
    : shelf_view_(new ShelfView(model, shelf)) {
  Shell::Get()->AddShellObserver(this);
}

ScrollableShelfView::~ScrollableShelfView() {
  Shell::Get()->RemoveShellObserver(this);
}

void ScrollableShelfView::Init() {
  shelf_view_->Init();

  // Although there is no animation for ScrollableShelfView, a layer is still
  // needed. Otherwise, the child view without its own layer will be painted on
  // RootView and RootView is beneath |opaque_background_| in ShelfWidget. As a
  // result, the child view will not show.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Initialize the left arrow button.
  left_arrow_ = AddChildView(std::make_unique<ScrollArrowView>(
      ScrollArrowView::kLeft, GetShelf()->IsHorizontalAlignment(), this));

  // Initialize the right arrow button.
  right_arrow_ = AddChildView(std::make_unique<ScrollArrowView>(
      ScrollArrowView::kRight, GetShelf()->IsHorizontalAlignment(), this));

  // Initialize the shelf container view.
  shelf_container_view_ =
      AddChildView(std::make_unique<ShelfContainerView>(shelf_view_));
  shelf_container_view_->Initialize();
}

views::View* ScrollableShelfView::GetShelfContainerViewForTest() {
  return shelf_container_view_;
}

int ScrollableShelfView::CalculateScrollUpperBound() const {
  if (layout_strategy_ == kNotShowArrowButtons)
    return 0;

  // Calculate the length of the available space.
  int available_length = space_for_icons_ - 2 * kEndPadding;

  // Calculate the length of the preferred size.
  const gfx::Size shelf_preferred_size(
      shelf_container_view_->GetPreferredSize());
  const int preferred_length =
      (GetShelf()->IsHorizontalAlignment() ? shelf_preferred_size.width()
                                           : shelf_preferred_size.height());

  return std::max(0, preferred_length - available_length);
}

float ScrollableShelfView::CalculateClampedScrollOffset(float scroll) const {
  const float old_scroll = GetShelf()->IsHorizontalAlignment()
                               ? scroll_offset_.x()
                               : scroll_offset_.y();
  const float scroll_upper_bound = CalculateScrollUpperBound();
  scroll = std::min(scroll_upper_bound, std::max(0.f, old_scroll + scroll));
  return scroll;
}

void ScrollableShelfView::StartShelfScrollAnimation(float scroll_distance) {
  const gfx::Transform current_transform = shelf_view_->GetTransform();
  gfx::Transform reverse_transform = current_transform;
  if (ShouldAdaptToRTL())
    scroll_distance = -scroll_distance;
  if (GetShelf()->IsHorizontalAlignment())
    reverse_transform.Translate(gfx::Vector2dF(scroll_distance, 0));
  else
    reverse_transform.Translate(gfx::Vector2dF(0, scroll_distance));
  shelf_view_->layer()->SetTransform(reverse_transform);
  ui::ScopedLayerAnimationSettings animation_settings(
      shelf_view_->layer()->GetAnimator());
  animation_settings.SetTweenType(gfx::Tween::EASE_OUT);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  shelf_view_->layer()->SetTransform(current_transform);
}

void ScrollableShelfView::UpdateLayoutStrategy(int available_length) {
  gfx::Size preferred_size = GetPreferredSize();
  int preferred_length = GetShelf()->IsHorizontalAlignment()
                             ? preferred_size.width()
                             : preferred_size.height();
  preferred_length += 2 * kEndPadding;

  int scroll_length = GetShelf()->IsHorizontalAlignment() ? scroll_offset_.x()
                                                          : scroll_offset_.y();

  if (preferred_length <= available_length) {
    // Enough space to accommodate all of shelf buttons. So hide arrow buttons.
    layout_strategy_ = kNotShowArrowButtons;
  } else if (scroll_length == 0) {
    // No invisible shelf buttons at the left side. So hide the left button.
    layout_strategy_ = kShowRightArrowButton;
  } else if (scroll_length == CalculateScrollUpperBound()) {
    // If there is no invisible shelf button at the right side, hide the right
    // button.
    layout_strategy_ = kShowLeftArrowButton;
  } else {
    // There are invisible shelf buttons at both sides. So show two buttons.
    layout_strategy_ = kShowButtons;
  }
}

bool ScrollableShelfView::ShouldAdaptToRTL() const {
  return base::i18n::IsRTL() && GetShelf()->IsHorizontalAlignment();
}

Shelf* ScrollableShelfView::GetShelf() {
  return const_cast<Shelf*>(
      const_cast<const ScrollableShelfView*>(this)->GetShelf());
}

const Shelf* ScrollableShelfView::GetShelf() const {
  return shelf_view_->shelf();
}

gfx::Size ScrollableShelfView::CalculatePreferredSize() const {
  return shelf_container_view_->GetPreferredSize();
}

void ScrollableShelfView::Layout() {
  const bool is_horizontal = GetShelf()->IsHorizontalAlignment();
  const int adjusted_length =
      (is_horizontal ? width() : height()) - 2 * kAppIconGroupMargin;
  UpdateLayoutStrategy(adjusted_length);

  // Both |left_padding| and |right_padding| include kAppIconGroupMargin.
  gfx::Insets padding_insets = CalculateEdgePadding();
  const int left_padding = padding_insets.left();
  const int right_padding = padding_insets.right();
  space_for_icons_ =
      (is_horizontal ? width() : height()) - left_padding - right_padding;

  gfx::Size shelf_button_size(ShelfConstants::button_size(),
                              ShelfConstants::button_size());
  gfx::Size arrow_button_size(GetArrowButtonSize(), GetArrowButtonSize());
  gfx::Rect shelf_container_bounds = gfx::Rect(size());

  // Transpose and layout as if it is horizontal.
  if (!is_horizontal)
    shelf_container_bounds.Transpose();

  // The bounds of |left_arrow_| and |right_arrow_| in the parent coordinates.
  gfx::Rect left_arrow_bounds;
  gfx::Rect right_arrow_bounds;

  // Calculates the bounds of the left arrow button. If the left arrow button
  // should not show, |left_arrow_bounds| should be empty.
  if (layout_strategy_ == kShowLeftArrowButton ||
      layout_strategy_ == kShowButtons) {
    left_arrow_bounds = gfx::Rect(shelf_button_size);
    left_arrow_bounds.Offset(left_padding, 0);
    left_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(
        ShelfConstants::button_size() + GetDistanceToArrowButton(), 0, 0, 0);
  }

  if (layout_strategy_ == kShowRightArrowButton ||
      layout_strategy_ == kShowButtons) {
    gfx::Point right_arrow_start_point(shelf_container_bounds.right() -
                                           ShelfConstants::button_size() -
                                           right_padding,
                                       0);
    right_arrow_bounds = gfx::Rect(right_arrow_start_point, shelf_button_size);
    right_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(
        0, 0, ShelfConstants::button_size() + GetDistanceToArrowButton(), 0);
  }

  shelf_container_bounds.Inset(left_padding + kEndPadding, 0,
                               right_padding + kEndPadding, 0);

  // Adjust the bounds when not showing in the horizontal
  // alignment.tShelf()->IsHorizontalAlignment()) {
  if (!is_horizontal) {
    left_arrow_bounds.Transpose();
    right_arrow_bounds.Transpose();
    shelf_container_bounds.Transpose();
  }

  // Layout |left_arrow| if it should show.
  left_arrow_->SetVisible(!left_arrow_bounds.IsEmpty());
  if (left_arrow_->GetVisible())
    left_arrow_->SetBoundsRect(left_arrow_bounds);

  // Layout |right_arrow| if it should show.
  right_arrow_->SetVisible(!right_arrow_bounds.IsEmpty());
  if (right_arrow_->GetVisible())
    right_arrow_->SetBoundsRect(right_arrow_bounds);

  // Layout |shelf_container_view_|.
  shelf_container_view_->SetBoundsRect(shelf_container_bounds);

  // When the left button shows, the origin of |shelf_container_view_| changes.
  // So translate |shelf_container_view| to show the shelf view correctly.
  gfx::Vector2d translate_vector;
  if (!left_arrow_bounds.IsEmpty()) {
    translate_vector =
        GetShelf()->IsHorizontalAlignment()
            ? gfx::Vector2d(
                  shelf_container_bounds.x() - kEndPadding - left_padding, 0)
            : gfx::Vector2d(
                  0, shelf_container_bounds.y() - kEndPadding - left_padding);
  }

  gfx::Vector2dF total_offset = scroll_offset_ + translate_vector;
  if (ShouldAdaptToRTL())
    total_offset = -total_offset;

  shelf_container_view_->TranslateShelfView(total_offset);
}

void ScrollableShelfView::ChildPreferredSizeChanged(views::View* child) {
  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(0, /*animate=*/false);
  else
    ScrollByYOffset(0, /*animate=*/false);
}

void ScrollableShelfView::OnMouseEvent(ui::MouseEvent* event) {
  // The mouse event's location may be outside of ShelfView but within the
  // bounds of the ScrollableShelfView. Meanwhile, ScrollableShelfView should
  // handle the mouse event consistently with ShelfView. To achieve this,
  // we simply redirect |event| to ShelfView.
  gfx::Point location_in_shelf_view = event->location();
  View::ConvertPointToTarget(this, shelf_view_, &location_in_shelf_view);
  event->set_location(location_in_shelf_view);
  shelf_view_->OnMouseEvent(event);
}

void ScrollableShelfView::OnGestureEvent(ui::GestureEvent* event) {
  if (ShouldHandleGestures(*event))
    HandleGestureEvent(event);
  else
    shelf_view_->HandleGestureEvent(event);
}

const char* ScrollableShelfView::GetClassName() const {
  return "ScrollableShelfView";
}

int ScrollableShelfView::GetArrowButtonSize() {
  static int kArrowButtonSize = ShelfConstants::control_size();
  return kArrowButtonSize;
}

void ScrollableShelfView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  // Verfies that |sender| is either |left_arrow_| or |right_arrow_|.
  views::View* sender_view = sender;
  DCHECK((sender_view == left_arrow_) || (sender_view == right_arrow_));

  // Implement the arrow button handler in the same way with the gesture
  // scrolling. The key is to calculate the suitable scroll distance.
  int offset = space_for_icons_ - 2 * GetUnit();
  DCHECK_GT(offset, 0);

  // If |forward| is true, scroll the scrollable shelf view rightward.
  const bool forward = sender_view == right_arrow_;
  if (!forward)
    offset = -offset;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, true);
  else
    ScrollByYOffset(offset, true);
}

void ScrollableShelfView::OnShelfAlignmentChanged(aura::Window* root_window) {
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  left_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  right_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  scroll_offset_ = gfx::Vector2dF();
  Layout();
}

gfx::Insets ScrollableShelfView::CalculateEdgePadding() const {
  const int available_size_for_app_icons =
      (GetShelf()->IsHorizontalAlignment() ? width() : height()) -
      2 * kAppIconGroupMargin;
  const int icons_size = shelf_view_->GetSizeOfAppIcons(
      shelf_view_->number_of_visible_apps(), false);

  gfx::Insets padding_insets(/*vertical= */ 0,
                             /*horizontal= */ kAppIconGroupMargin);
  int gap = layout_strategy_ == kNotShowArrowButtons
                ? available_size_for_app_icons - icons_size
                : available_size_for_app_icons % GetUnit();
  padding_insets.set_left(padding_insets.left() + gap / 2);
  padding_insets.set_right(padding_insets.right() +
                           (gap % 2 ? gap / 2 + 1 : gap / 2));

  return padding_insets;
}

bool ScrollableShelfView::ShouldHandleGestures(const ui::GestureEvent& event) {
  if (!cross_main_axis_scrolling_ && !event.IsScrollGestureEvent())
    return true;

  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    CHECK_EQ(false, cross_main_axis_scrolling_);

    float main_offset = event.details().scroll_x_hint();
    float cross_offset = event.details().scroll_y_hint();
    if (!GetShelf()->IsHorizontalAlignment())
      std::swap(main_offset, cross_offset);

    cross_main_axis_scrolling_ = std::abs(main_offset) < std::abs(cross_offset);
  }

  // Gesture scrollings perpendicular to the main axis should be handled by
  // ShelfView.
  bool should_handle_gestures = !cross_main_axis_scrolling_;

  if (event.type() == ui::ET_GESTURE_END)
    cross_main_axis_scrolling_ = false;

  return should_handle_gestures;
}

void ScrollableShelfView::HandleGestureEvent(ui::GestureEvent* event) {
  if (ProcessGestureEvent(*event))
    event->SetHandled();
}

bool ScrollableShelfView::ProcessGestureEvent(const ui::GestureEvent& event) {
  if (layout_strategy_ == kNotShowArrowButtons)
    return true;

  // Handle scroll-related events, but don't do anything special for begin and
  // end.
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    return true;
  }

  // Make sure that no visible shelf button is partially shown after gestures.
  if (event.type() == ui::ET_GESTURE_END ||
      event.type() == ui::ET_GESTURE_SCROLL_END) {
    int current_scroll_distance = GetShelf()->IsHorizontalAlignment()
                                      ? scroll_offset_.x()
                                      : scroll_offset_.y();
    const int residue = current_scroll_distance % GetUnit();

    // if it does not need to adjust the location of the shelf view,
    // return early.
    if (current_scroll_distance == CalculateScrollUpperBound() || residue == 0)
      return true;

    int offset =
        residue > GetGestureDragThreshold() ? GetUnit() - residue : -residue;
    if (GetShelf()->IsHorizontalAlignment())
      ScrollByXOffset(offset, /*animate=*/true);
    else
      ScrollByYOffset(offset, /*animate=*/true);
    return true;
  }

  if (event.type() != ui::ET_GESTURE_SCROLL_UPDATE)
    return false;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(-event.details().scroll_x(), /*animate=*/false);
  else
    ScrollByYOffset(-event.details().scroll_y(), /*animate=*/false);
  return true;
}

void ScrollableShelfView::ScrollByXOffset(float x_offset, bool animating) {
  const float old_x = scroll_offset_.x();
  const float x = CalculateClampedScrollOffset(x_offset);
  scroll_offset_.set_x(x);
  Layout();
  const float diff = x - old_x;
  if (animating)
    StartShelfScrollAnimation(diff);
}

void ScrollableShelfView::ScrollByYOffset(float y_offset, bool animating) {
  const int old_y = scroll_offset_.y();
  const int y = CalculateClampedScrollOffset(y_offset);
  scroll_offset_.set_y(y);
  Layout();
  const float diff = y - old_y;
  if (animating)
    StartShelfScrollAnimation(diff);
}

}  // namespace ash
