// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/numerics/ranges.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/focus/focus_search.h"

namespace ash {

namespace {

// Padding between the arrow button and the end of ScrollableShelf. It is
// applied when the arrow button shows.
constexpr int kArrowButtonEndPadding = 6;

// Padding between the the shelf container view and the arrow button (if any).
constexpr int kDistanceToArrowButton = 2;

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
  return ShelfConfig::Get()->button_size() +
         ShelfConfig::Get()->button_spacing();
}

// Length of the fade in/out zone.
int GetFadeZoneLength() {
  return GetUnit() - kArrowButtonGroupWidth;
}

// Decides whether the current first visible shelf icon of the scrollable shelf
// should be hidden or fully shown when gesture scroll ends.
int GetGestureDragThreshold() {
  return ShelfConfig::Get()->button_size() / 2;
}

// Returns the padding between the app icon and the end of the ScrollableShelf.
int GetAppIconEndPadding() {
  if (Shell::Get()->tablet_mode_controller()->InTabletMode())
    return 4;
  return 0;
}

// Calculates the padding for overflow.
int CalculateOverflowPadding(int available_size) {
  return (available_size - kArrowButtonGroupWidth -
          ShelfConfig::Get()->button_size() - GetAppIconEndPadding()) %
         GetUnit();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GradientLayerDelegate

class ScrollableShelfView::GradientLayerDelegate : public ui::LayerDelegate {
 public:
  struct FadeZone {
    // Bounds of the fade in/out zone.
    gfx::Rect zone_rect;

    // Specifies the type of FadeZone: fade in or fade out.
    bool fade_in = false;

    // Indicates the drawing direction.
    bool is_horizontal = false;
  };

  GradientLayerDelegate() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~GradientLayerDelegate() override { layer_.set_delegate(nullptr); }

  void set_fade_zone(const FadeZone& fade_zone) { fade_zone_ = fade_zone; }
  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    const gfx::Size size = layer()->size();

    views::PaintInfo paint_info =
        views::PaintInfo::CreateRootPaintInfo(context, size);
    const auto& paint_recording_size = paint_info.paint_recording_size();

    // Pass the scale factor when constructing PaintRecorder so the MaskLayer
    // size is not incorrectly rounded (see https://crbug.com/921274).
    ui::PaintRecorder recorder(
        context, paint_info.paint_recording_size(),
        static_cast<float>(paint_recording_size.width()) / size.width(),
        static_cast<float>(paint_recording_size.height()) / size.height(),
        nullptr);

    gfx::Point start_point;
    gfx::Point end_point;
    if (fade_zone_.is_horizontal) {
      start_point = gfx::Point(fade_zone_.zone_rect.x(), 0);
      end_point = gfx::Point(fade_zone_.zone_rect.right(), 0);
    } else {
      start_point = gfx::Point(0, fade_zone_.zone_rect.y());
      end_point = gfx::Point(0, fade_zone_.zone_rect.bottom());
    }

    gfx::Canvas* canvas = recorder.canvas();
    canvas->DrawColor(SK_ColorBLACK, SkBlendMode::kSrc);
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setAntiAlias(false);
    flags.setShader(gfx::CreateGradientShader(
        start_point, end_point,
        fade_zone_.fade_in ? SK_ColorTRANSPARENT : SK_ColorBLACK,
        fade_zone_.fade_in ? SK_ColorBLACK : SK_ColorTRANSPARENT));

    canvas->DrawRect(fade_zone_.zone_rect, flags);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;
  FadeZone fade_zone_;

  DISALLOW_COPY_AND_ASSIGN(GradientLayerDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfFocusSearch

class ScrollableShelfFocusSearch : public views::FocusSearch {
 public:
  explicit ScrollableShelfFocusSearch(
      ScrollableShelfView* scrollable_shelf_view)
      : FocusSearch(/*root=*/nullptr,
                    /*cycle=*/true,
                    /*accessibility_mode=*/true),
        scrollable_shelf_view_(scrollable_shelf_view) {}

  ~ScrollableShelfFocusSearch() override = default;

  // views::FocusSearch
  views::View* FindNextFocusableView(
      views::View* starting_view,
      FocusSearch::SearchDirection search_direction,
      FocusSearch::TraversalDirection traversal_direction,
      FocusSearch::StartingViewPolicy check_starting_view,
      FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override {
    std::vector<views::View*> focusable_views;
    ShelfView* shelf_view = scrollable_shelf_view_->shelf_view();

    for (int i = shelf_view->first_visible_index();
         i <= shelf_view->last_visible_index(); ++i) {
      focusable_views.push_back(shelf_view->view_model()->view_at(i));
    }

    int start_index = 0;
    for (size_t i = 0; i < focusable_views.size(); ++i) {
      if (focusable_views[i] == starting_view) {
        start_index = i;
        break;
      }
    }

    int new_index =
        start_index +
        (search_direction == FocusSearch::SearchDirection::kBackwards ? -1 : 1);

    // Scrolls to the new page if the focused shelf item is not tappable
    // on the current page.
    if (new_index < 0)
      new_index = focusable_views.size() - 1;
    else if (new_index >= static_cast<int>(focusable_views.size()))
      new_index = 0;
    else if (new_index < scrollable_shelf_view_->first_tappable_app_index())
      scrollable_shelf_view_->ScrollToNewPage(/*forward=*/false);
    else if (new_index > scrollable_shelf_view_->last_tappable_app_index())
      scrollable_shelf_view_->ScrollToNewPage(/*forward=*/true);

    return focusable_views[new_index];
  }

 private:
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfFocusSearch);
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfView

ScrollableShelfView::ScrollableShelfView(ShelfModel* model, Shelf* shelf)
    : shelf_view_(new ShelfView(model, shelf)) {
  Shell::Get()->AddShellObserver(this);
  set_allow_deactivate_on_esc(true);
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
      ScrollArrowView::kLeft, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  // Initialize the right arrow button.
  right_arrow_ = AddChildView(std::make_unique<ScrollArrowView>(
      ScrollArrowView::kRight, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  // Initialize the shelf container view.
  shelf_container_view_ =
      AddChildView(std::make_unique<ShelfContainerView>(shelf_view_));
  shelf_container_view_->Initialize();

  gradient_layer_delegate_ = std::make_unique<GradientLayerDelegate>();
  layer()->SetMaskLayer(gradient_layer_delegate_->layer());

  focus_search_ = std::make_unique<ScrollableShelfFocusSearch>(this);
}

void ScrollableShelfView::OnFocusRingActivationChanged(bool activated) {
  if (activated) {
    focus_ring_activated_ = true;
    SetPaneFocusAndFocusDefault();

    // Clears the gradient shader when the focus ring shows.
    GradientLayerDelegate::FadeZone fade_zone = {gfx::Rect(), false, false};
    gradient_layer_delegate_->set_fade_zone(fade_zone);
    gradient_layer_delegate_->layer()->SetBounds(layer()->bounds());
    SchedulePaint();
  } else {
    // Shows the gradient shader when the focus ring is disabled.
    focus_ring_activated_ = false;
    UpdateGradientZone();
  }
}

void ScrollableShelfView::ScrollToNewPage(bool forward) {
  float offset = CalculatePageScrollingOffset(forward);
  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, /*animating=*/true);
  else
    ScrollByYOffset(offset, /*animating=*/true);
}

views::FocusSearch* ScrollableShelfView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* ScrollableShelfView::GetFocusTraversableParent() {
  return parent()->GetFocusTraversable();
}

views::View* ScrollableShelfView::GetFocusTraversableParentView() {
  return this;
}

views::View* ScrollableShelfView::GetDefaultFocusableChild() {
  // Adapts |scroll_offset_| to show the view properly right after the focus
  // ring is enabled.

  if (default_last_focusable_child_) {
    scroll_offset_ = GetShelf()->IsHorizontalAlignment()
                         ? gfx::Vector2dF(CalculateScrollUpperBound(), 0)
                         : gfx::Vector2dF(0, CalculateScrollUpperBound());
    Layout();
    return FindLastFocusableChild();
  } else {
    scroll_offset_ = gfx::Vector2dF();
    Layout();
    return FindFirstFocusableChild();
  }
}

views::View* ScrollableShelfView::GetShelfContainerViewForTest() {
  return shelf_container_view_;
}

int ScrollableShelfView::CalculateScrollUpperBound() const {
  if (layout_strategy_ == kNotShowArrowButtons)
    return 0;

  // Calculate the length of the available space.
  int available_length = space_for_icons_ - 2 * GetAppIconEndPadding();

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
  scroll = base::ClampToRange(scroll, 0.0f, scroll_upper_bound);
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
  preferred_length += 2 * GetAppIconEndPadding();

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

bool ScrollableShelfView::ShouldApplyDisplayCentering() const {
  if (layout_strategy_ != kNotShowArrowButtons)
    return false;

  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetWidget()->GetNativeWindow());
  const int display_size_primary = GetShelf()->PrimaryAxisValue(
      display_bounds.width(), display_bounds.height());
  StatusAreaWidget* status_widget =
      GetShelf()->shelf_widget()->status_area_widget();
  const int status_widget_size = GetShelf()->PrimaryAxisValue(
      status_widget->GetWindowBoundsInScreen().width(),
      status_widget->GetWindowBoundsInScreen().height());

  // An easy way to check whether the apps fit at the exact center of the
  // screen is to imagine that we have another status widget on the other
  // side (the status widget is always bigger than the home button plus
  // the back button if applicable) and see if the apps can fit in the middle
  int available_space_for_screen_centering =
      display_size_primary -
      2 * (status_widget_size + ShelfConfig::Get()->app_icon_group_margin());
  return available_space_for_screen_centering >
         shelf_view_->GetSizeOfAppIcons(shelf_view_->number_of_visible_apps(),
                                        false);
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
  const int adjusted_length = (is_horizontal ? width() : height()) -
                              2 * ShelfConfig::Get()->app_icon_group_margin();
  UpdateLayoutStrategy(adjusted_length);

  // Both |left_padding| and |right_padding| include the app icon group margin.
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
      left_padding + (left_arrow_bounds.IsEmpty() ? GetAppIconEndPadding() : 0),
      0,
      right_padding +
          (right_arrow_bounds.IsEmpty() ? GetAppIconEndPadding() : 0),
      0);

  // Adjust the bounds when not showing in the horizontal
  // alignment.tShelf()->IsHorizontalAlignment()) {
  if (!is_horizontal) {
    left_arrow_bounds.Transpose();
    right_arrow_bounds.Transpose();
    shelf_container_bounds.Transpose();
  }

  // Check the change in |right_arrow_|'s bounds or the visibility.
  const bool is_right_arrow_changed =
      (right_arrow_->bounds() != right_arrow_bounds) ||
      (!right_arrow_bounds.IsEmpty() && !right_arrow_->GetVisible());

  // Layout |left_arrow| if it should show.
  left_arrow_->SetVisible(!left_arrow_bounds.IsEmpty());
  if (left_arrow_->GetVisible())
    left_arrow_->SetBoundsRect(left_arrow_bounds);

  // Layout |right_arrow| if it should show.
  right_arrow_->SetVisible(!right_arrow_bounds.IsEmpty());
  if (right_arrow_->GetVisible())
    right_arrow_->SetBoundsRect(right_arrow_bounds);

  if (gradient_layer_delegate_->layer()->bounds() != layer()->bounds())
    gradient_layer_delegate_->layer()->SetBounds(layer()->bounds());

  // Bounds of the gradient zone are relative to the location of arrow buttons.
  // So updates the gradient zone after the bounds of arrow buttons are set.
  // The gradient zone is not painted when the focus ring shows in order to
  // display the focus ring correctly.
  if (is_right_arrow_changed && !focus_ring_activated_)
    UpdateGradientZone();

  // Layout |shelf_container_view_|.
  shelf_container_view_->SetBoundsRect(shelf_container_bounds);

  // When the left button shows, the origin of |shelf_container_view_| changes.
  // So translate |shelf_container_view| to show the shelf view correctly.
  gfx::Vector2d translate_vector;
  if (!left_arrow_bounds.IsEmpty()) {
    translate_vector =
        GetShelf()->IsHorizontalAlignment()
            ? gfx::Vector2d(shelf_container_bounds.x() -
                                GetAppIconEndPadding() - left_padding,
                            0)
            : gfx::Vector2d(0, shelf_container_bounds.y() -
                                   GetAppIconEndPadding() - left_padding);
  }
  gfx::Vector2dF total_offset = scroll_offset_ + translate_vector;
  if (ShouldAdaptToRTL())
    total_offset = -total_offset;

  shelf_container_view_->TranslateShelfView(total_offset);

  UpdateTappableIconIndices();
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

void ScrollableShelfView::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {}

void ScrollableShelfView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event,
                                        views::InkDrop* ink_drop) {
  // Verfies that |sender| is either |left_arrow_| or |right_arrow_|.
  views::View* sender_view = sender;
  DCHECK((sender_view == left_arrow_) || (sender_view == right_arrow_));

  ScrollToNewPage(sender_view == right_arrow_);
}

void ScrollableShelfView::OnShelfAlignmentChanged(aura::Window* root_window) {
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  left_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  right_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  scroll_offset_ = gfx::Vector2dF();
  Layout();
}

gfx::Insets ScrollableShelfView::CalculateEdgePadding() const {
  // Display centering alignment
  if (ShouldApplyDisplayCentering())
    return CalculatePaddingForDisplayCentering();

  const int icons_size = shelf_view_->GetSizeOfAppIcons(
      shelf_view_->number_of_visible_apps(), false);
  gfx::Insets padding_insets(
      /*vertical= */ 0,
      /*horizontal= */ ShelfConfig::Get()->app_icon_group_margin());

  const int available_size_for_app_icons =
      (GetShelf()->IsHorizontalAlignment() ? width() : height()) -
      2 * ShelfConfig::Get()->app_icon_group_margin();

  int gap =
      layout_strategy_ == kNotShowArrowButtons
          ? available_size_for_app_icons - icons_size  // shelf centering
          : CalculateOverflowPadding(available_size_for_app_icons);  // overflow

  padding_insets.set_left(padding_insets.left() + gap / 2);
  padding_insets.set_right(padding_insets.right() +
                           (gap % 2 ? gap / 2 + 1 : gap / 2));

  return padding_insets;
}

gfx::Insets ScrollableShelfView::CalculatePaddingForDisplayCentering() const {
  const int icons_size = shelf_view_->GetSizeOfAppIcons(
      shelf_view_->number_of_visible_apps(), false);
  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetWidget()->GetNativeWindow());
  const int display_size_primary = GetShelf()->PrimaryAxisValue(
      display_bounds.width(), display_bounds.height());
  const int gap = (display_size_primary - icons_size) / 2;

  // Calculates paddings in view coordinates.
  const gfx::Rect screen_bounds = GetBoundsInScreen();
  const int left_padding = gap - GetShelf()->PrimaryAxisValue(
                                     screen_bounds.x() - display_bounds.x(),
                                     screen_bounds.y() - display_bounds.y());
  const int right_padding =
      gap - GetShelf()->PrimaryAxisValue(
                display_bounds.right() - screen_bounds.right(),
                display_bounds.bottom() - screen_bounds.bottom());

  return gfx::Insets(0, left_padding, 0, right_padding);
}

bool ScrollableShelfView::ShouldHandleGestures(const ui::GestureEvent& event) {
  // ScrollableShelfView only handles the gesture scrolling along the main axis.
  // For other gesture events, including the scrolling across the main axis,
  // they are handled by ShelfView.

  if (scroll_status_ == kNotInScroll && !event.IsScrollGestureEvent())
    return false;

  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    CHECK_EQ(scroll_status_, kNotInScroll);

    float main_offset = event.details().scroll_x_hint();
    float cross_offset = event.details().scroll_y_hint();
    if (!GetShelf()->IsHorizontalAlignment())
      std::swap(main_offset, cross_offset);

    scroll_status_ = std::abs(main_offset) < std::abs(cross_offset)
                         ? kAcrossMainAxisScroll
                         : kAlongMainAxisScroll;
  }

  bool should_handle_gestures = scroll_status_ == kAlongMainAxisScroll;

  if (scroll_status_ == kAlongMainAxisScroll &&
      event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    scroll_offset_before_main_axis_scrolling_ = scroll_offset_;
    layout_strategy_before_main_axis_scrolling_ = layout_strategy_;
  }

  if (event.type() == ui::ET_GESTURE_END) {
    scroll_status_ = kNotInScroll;

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

    int actual_scroll_distance = GetActualScrollOffset();

    // Returns early when it does not need to adjust the shelf view's location.
    if (actual_scroll_distance == CalculateScrollUpperBound())
      return true;

    const int residue = actual_scroll_distance % GetUnit();
    int offset =
        residue > GetGestureDragThreshold() ? GetUnit() - residue : -residue;

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
                 ShelfConfig::Get()->button_size() - GetAppIconEndPadding();
  if (layout_strategy_ == kShowRightArrowButton)
    offset -= (kArrowButtonGroupWidth - GetAppIconEndPadding());
  DCHECK_GT(offset, 0);

  if (!forward)
    offset = -offset;

  return offset;
}

void ScrollableShelfView::UpdateGradientZone() {
  gfx::Rect zone_rect;
  bool fade_in;
  const int zone_length = GetFadeZoneLength();
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();

  // Calculates the bounds of the gradient zone based on the arrow buttons'
  // location.
  if (right_arrow_->GetVisible()) {
    const gfx::Rect right_arrow_bounds = right_arrow_->bounds();
    zone_rect = is_horizontal_alignment
                    ? gfx::Rect(right_arrow_bounds.x() - zone_length, 0,
                                zone_length, height())
                    : gfx::Rect(0, right_arrow_bounds.y() - zone_length,
                                width(), zone_length);
    fade_in = false;
  } else if (left_arrow_->GetVisible()) {
    const gfx::Rect left_arrow_bounds = left_arrow_->bounds();
    zone_rect =
        is_horizontal_alignment
            ? gfx::Rect(left_arrow_bounds.right(), 0, zone_length, height())
            : gfx::Rect(0, left_arrow_bounds.bottom(), width(), zone_length);
    fade_in = true;
  } else {
    zone_rect = gfx::Rect();
    fade_in = false;
  }

  GradientLayerDelegate::FadeZone fade_zone = {zone_rect, fade_in,
                                               is_horizontal_alignment};
  gradient_layer_delegate_->set_fade_zone(fade_zone);
  SchedulePaint();
}

int ScrollableShelfView::GetActualScrollOffset() const {
  int scroll_distance = GetShelf()->IsHorizontalAlignment()
                            ? scroll_offset_.x()
                            : scroll_offset_.y();
  if (left_arrow_->GetVisible())
    scroll_distance += (kArrowButtonGroupWidth - GetAppIconEndPadding());
  return scroll_distance;
}

void ScrollableShelfView::UpdateTappableIconIndices() {
  if (layout_strategy_ == kNotShowArrowButtons) {
    first_tappable_app_index_ = shelf_view_->first_visible_index();
    last_tappable_app_index_ = shelf_view_->last_visible_index();
    return;
  }

  int actual_scroll_distance = GetActualScrollOffset();
  int shelf_container_available_space =
      (GetShelf()->IsHorizontalAlignment() ? shelf_container_view_->width()
                                           : shelf_container_view_->height()) -
      GetFadeZoneLength();
  if (layout_strategy_ == kShowRightArrowButton ||
      layout_strategy_ == kShowButtons) {
    first_tappable_app_index_ = actual_scroll_distance / GetUnit();
    last_tappable_app_index_ =
        first_tappable_app_index_ + shelf_container_available_space / GetUnit();
  } else {
    DCHECK_EQ(layout_strategy_, kShowLeftArrowButton);
    last_tappable_app_index_ = shelf_view_->last_visible_index();
    first_tappable_app_index_ =
        last_tappable_app_index_ - shelf_container_available_space / GetUnit();
  }
}

views::View* ScrollableShelfView::FindFirstFocusableChild() {
  return shelf_view_->view_model()->view_at(shelf_view_->first_visible_index());
}

views::View* ScrollableShelfView::FindLastFocusableChild() {
  return shelf_view_->view_model()->view_at(shelf_view_->last_visible_index());
}

}  // namespace ash
