// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace ash {

namespace {

// Padding between the arrow button and the end of ScrollableShelf. It is
// applied when the arrow button shows.
constexpr int kArrowButtonEndPadding = 6;

// Padding between the the shelf container view and the arrow button (if any).
constexpr int kDistanceToArrowButton = 2;

// Padding between the app icon and the end of the ScrollableShelf. It is
// applied when the arrow button not shows.
constexpr int kAppIconEndPadding = 4;

// Size of the arrow button.
constexpr int kArrowButtonSize = 20;

// The distance between ShelfContainerView and the end of ScrollableShelf when
// the arrow button shows.
constexpr int kArrowButtonGroupWidth =
    kArrowButtonSize + kArrowButtonEndPadding + kDistanceToArrowButton;

// The gesture fling event with the velocity smaller than the threshold will be
// neglected.
constexpr int kFlingVelocityThreshold = 1000;

// Sum of the shelf button size and the gap between shelf buttons.
int GetUnit() {
  return ShelfConstants::button_size() + ShelfConstants::button_spacing();
}

// Decides whether the current first visible shelf icon of the scrollable shelf
// should be hidden or fully shown when gesture scroll ends.
int GetGestureDragThreshold() {
  return ShelfConstants::button_size() / 2;
}

// Calculates the padding for overflow.
int CalculateOverflowPadding(int available_size) {
  return (available_size - kArrowButtonGroupWidth -
          ShelfConstants::button_size() - kAppIconEndPadding) %
         GetUnit();
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
  int available_length = space_for_icons_ - 2 * kAppIconEndPadding;

  // Calculate the length of the preferred size.
  const gfx::Size shelf_preferred_size(
      shelf_container_view_->GetPreferredSize());
  const int preferred_length =
      (GetShelf()->IsHorizontalAlignment() ? shelf_preferred_size.width()
                                           : shelf_preferred_size.height());

  return std::max(0, preferred_length - available_length);
}

float ScrollableShelfView::CalculateClampedScrollOffset(float scroll) const {
  const float scroll_upper_bound = CalculateScrollUpperBound();
  scroll = std::min(scroll_upper_bound, std::max(0.f, scroll));
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
  preferred_length += 2 * kAppIconEndPadding;

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

  gfx::Size arrow_button_size(kArrowButtonSize, kArrowButtonSize);
  gfx::Rect shelf_container_bounds = gfx::Rect(size());

  // Transpose and layout as if it is horizontal.
  if (!is_horizontal)
    shelf_container_bounds.Transpose();

  gfx::Size arrow_button_group_size(kArrowButtonGroupWidth,
                                    shelf_container_bounds.height());

  // The bounds of |left_arrow_| and |right_arrow_| in the parent coordinates.
  gfx::Rect left_arrow_bounds;
  gfx::Rect right_arrow_bounds;

  // Calculates the bounds of the left arrow button. If the left arrow button
  // should not show, |left_arrow_bounds| should be empty.
  if (layout_strategy_ == kShowLeftArrowButton ||
      layout_strategy_ == kShowButtons) {
    left_arrow_bounds = gfx::Rect(arrow_button_group_size);
    left_arrow_bounds.Offset(left_padding, 0);
    left_arrow_bounds.Inset(kArrowButtonEndPadding, 0, kDistanceToArrowButton,
                            0);
    left_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(kArrowButtonGroupWidth, 0, 0, 0);
  }

  if (layout_strategy_ == kShowRightArrowButton ||
      layout_strategy_ == kShowButtons) {
    gfx::Point right_arrow_start_point(
        shelf_container_bounds.right() - right_padding - kArrowButtonGroupWidth,
        0);
    right_arrow_bounds =
        gfx::Rect(right_arrow_start_point, arrow_button_group_size);
    right_arrow_bounds.Inset(kDistanceToArrowButton, 0, kArrowButtonEndPadding,
                             0);
    right_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(0, 0, kArrowButtonGroupWidth, 0);
  }

  shelf_container_bounds.Inset(
      left_padding + (left_arrow_bounds.IsEmpty() ? kAppIconEndPadding : 0), 0,
      right_padding + (right_arrow_bounds.IsEmpty() ? kAppIconEndPadding : 0),
      0);

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
            ? gfx::Vector2d(shelf_container_bounds.x() - kAppIconEndPadding -
                                left_padding,
                            0)
            : gfx::Vector2d(0, shelf_container_bounds.y() - kAppIconEndPadding -
                                   left_padding);
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

void ScrollableShelfView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  // Verfies that |sender| is either |left_arrow_| or |right_arrow_|.
  views::View* sender_view = sender;
  DCHECK((sender_view == left_arrow_) || (sender_view == right_arrow_));

  float offset = CalculatePageScrollingOffset(sender_view == right_arrow_);
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
                : CalculateOverflowPadding(available_size_for_app_icons);
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

  if (should_handle_gestures && event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    scroll_offset_before_main_axis_scrolling_ = scroll_offset_;
    layout_strategy_before_main_axis_scrolling_ = layout_strategy_;
  }

  if (event.type() == ui::ET_GESTURE_END) {
    cross_main_axis_scrolling_ = false;

    if (should_handle_gestures) {
      scroll_offset_before_main_axis_scrolling_ = gfx::Vector2dF();
      layout_strategy_before_main_axis_scrolling_ = kNotShowArrowButtons;
    }
  }

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
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event.type() == ui::ET_GESTURE_SCROLL_END) {
    return true;
  }

  // Make sure that no visible shelf button is partially shown after gestures.
  if (event.type() == ui::ET_GESTURE_END) {
    // The type of scrolling offset is float to ensure that ScrollableShelfView
    // is responsive to slow gesture scrolling. However, when the gesture ends,
    // the scrolling offset should be floored.
    scroll_offset_ = gfx::ToFlooredVector2d(scroll_offset_);

    int current_scroll_distance = GetShelf()->IsHorizontalAlignment()
                                      ? scroll_offset_.x()
                                      : scroll_offset_.y();

    // Returns early when it does not need to adjust the shelf view's location.
    if (current_scroll_distance == CalculateScrollUpperBound())
      return true;

    const int residue = current_scroll_distance % GetUnit();
    int offset =
        residue > GetGestureDragThreshold() ? GetUnit() - residue : -residue;

    // In order to fully show the leftmost app icon when the left arrow button
    // shows.
    if (residue)
      offset -= (kArrowButtonGroupWidth - kAppIconEndPadding);

    // Returns early when it does not need to adjust the shelf view's location.
    if (!offset)
      return true;

    if (GetShelf()->IsHorizontalAlignment())
      ScrollByXOffset(offset, /*animate=*/true);
    else
      ScrollByYOffset(offset, /*animate=*/true);
    return true;
  }

  if (event.type() == ui::ET_SCROLL_FLING_START) {
    const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();

    int scroll_velocity = is_horizontal_alignment
                              ? event.details().velocity_x()
                              : event.details().velocity_y();
    if (abs(scroll_velocity) < kFlingVelocityThreshold)
      return false;

    layout_strategy_ = layout_strategy_before_main_axis_scrolling_;

    float page_scrolling_offset =
        CalculatePageScrollingOffset(scroll_velocity < 0);
    if (is_horizontal_alignment) {
      ScrollToXOffset(
          scroll_offset_before_main_axis_scrolling_.x() + page_scrolling_offset,
          true);
    } else {
      ScrollToYOffset(
          scroll_offset_before_main_axis_scrolling_.y() + page_scrolling_offset,
          true);
    }

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
  ScrollToXOffset(scroll_offset_.x() + x_offset, animating);
}

void ScrollableShelfView::ScrollByYOffset(float y_offset, bool animating) {
  ScrollToYOffset(scroll_offset_.y() + y_offset, animating);
}

void ScrollableShelfView::ScrollToXOffset(float x_target_offset,
                                          bool animating) {
  x_target_offset = CalculateClampedScrollOffset(x_target_offset);
  const float old_x = scroll_offset_.x();
  scroll_offset_.set_x(x_target_offset);
  Layout();
  const float diff = x_target_offset - old_x;

  if (animating)
    StartShelfScrollAnimation(diff);
}

void ScrollableShelfView::ScrollToYOffset(float y_target_offset,
                                          bool animating) {
  y_target_offset = CalculateClampedScrollOffset(y_target_offset);
  const int old_y = scroll_offset_.y();
  scroll_offset_.set_y(y_target_offset);
  Layout();
  const float diff = y_target_offset - old_y;
  if (animating)
    StartShelfScrollAnimation(diff);
}

float ScrollableShelfView::CalculatePageScrollingOffset(bool forward) const {
  // Implement the arrow button handler in the same way with the gesture
  // scrolling. The key is to calculate the suitable scroll distance.
  float offset = space_for_icons_ - kArrowButtonGroupWidth -
                 ShelfConstants::button_size() - kAppIconEndPadding;
  if (layout_strategy_ == kShowRightArrowButton)
    offset -= (kArrowButtonGroupWidth - kAppIconEndPadding);
  DCHECK_GT(offset, 0);

  if (!forward)
    offset = -offset;

  return offset;
}

}  // namespace ash
