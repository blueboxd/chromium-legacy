// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_targeter_delegate.h"

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
// ignored.
constexpr int kGestureFlingVelocityThreshold = 1000;

// The mouse wheel event (including touchpad scrolling) with the main axis
// offset smaller than the threshold will be ignored.
constexpr int KScrollOffsetThreshold = 20;

// Horizontal size of the tap areafor the overflow arrow button.
constexpr int kArrowButtonTapAreaHorizontal = 32;

// Length of the fade in/out zone.
constexpr int kGradientZoneLength = 26;

// Delay to show a new page of shelf icons.
constexpr base::TimeDelta kShelfPageFlipDelay =
    base::TimeDelta::FromMilliseconds(500);

// Histogram names for the scrollable shelf dragging metrics.
constexpr char kScrollDraggingTabletLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.TabletMode.LauncherVisible";
constexpr char kScrollDraggingTabletLauncherVisibleMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.TabletMode."
    "LauncherVisible";
constexpr char kScrollDraggingTabletLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.TabletMode.LauncherHidden";
constexpr char kScrollDraggingTabletLauncherHiddenMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.TabletMode."
    "LauncherHidden";
constexpr char kScrollDraggingClamshellLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.ClamshellMode.LauncherVisible";
constexpr char kScrollDraggingClamshellLauncherVisibleMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.ClamshellMode."
    "LauncherVisible";
constexpr char kScrollDraggingClamshellLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.ClamshellMode.LauncherHidden";
constexpr char kScrollDraggingClamshellLauncherHiddenMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.ClamshellMode."
    "LauncherHidden";

// Histogram names for the scrollable shelf animation smoothness metrics.
constexpr char kAnimationSmoothnessHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness";
constexpr char kAnimationSmoothnessTabletLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.TabletMode.LauncherVisible";
constexpr char kAnimationSmoothnessTabletLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.TabletMode.LauncherHidden";
constexpr char kAnimationSmoothnessClamshellLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.ClamshellMode.LauncherVisible";
constexpr char kAnimationSmoothnessClamshellLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.ClamshellMode.LauncherHidden";

// Sum of the shelf button size and the gap between shelf buttons.
int GetUnit() {
  return ShelfConfig::Get()->button_size() +
         ShelfConfig::Get()->button_spacing();
}

// Decides whether the current first visible shelf icon of the scrollable shelf
// should be hidden or fully shown when gesture scroll ends.
int GetGestureDragThreshold() {
  return ShelfConfig::Get()->button_size() / 2;
}

bool IsInTabletMode() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  // TabletModeController is destructed before ScrollableShelfView.
  if (!tablet_mode_controller || !tablet_mode_controller->InTabletMode())
    return false;

  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DragIconDropAnimationDelegate

class ScrollableShelfView::DragIconDropAnimationDelegate
    : public ui::ImplicitAnimationObserver {
 public:
  DragIconDropAnimationDelegate(views::View* original_view,
                                const gfx::RectF& src_bounds_in_screen,
                                const gfx::RectF& dst_bounds_in_screen,
                                std::unique_ptr<DragImageView> proxy_view)
      : original_view_(original_view),
        src_bounds_in_screen_(src_bounds_in_screen),
        dst_bounds_in_screen_(dst_bounds_in_screen),
        proxy_view_(std::move(proxy_view)) {}
  ~DragIconDropAnimationDelegate() override = default;

  DragIconDropAnimationDelegate(const DragIconDropAnimationDelegate&) = delete;
  DragIconDropAnimationDelegate& operator=(
      const DragIconDropAnimationDelegate&) = delete;

  void StartAnimation() {
    ui::ScopedLayerAnimationSettings animation_settings(
        proxy_view_->layer()->GetAnimator());
    animation_settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation_settings.AddObserver(this);

    proxy_view_->layer()->SetTransform(gfx::TransformBetweenRects(
        src_bounds_in_screen_, dst_bounds_in_screen_));
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    StopObserving();

    // Destructs the proxy image view and shows the original drag view at the
    // end of animation.
    original_view_->layer()->SetOpacity(1.0f);
    proxy_view_.reset();
  }

 private:
  // Original app icon being dragged in ShelfView.
  views::View* const original_view_ = nullptr;

  // Bounds of the DragImageView, aka the DragIconProxy created by the user of
  // ApplicationDragAndDropHost.
  gfx::RectF const src_bounds_in_screen_;
  // Bounds of the ShelfAppButton which DragImageView is imitating.
  gfx::RectF const dst_bounds_in_screen_;
  // Placeholder icon representing |original_view_| that moves with the pointer
  // while being dragged.
  std::unique_ptr<DragImageView> proxy_view_;
};

////////////////////////////////////////////////////////////////////////////////
// GradientLayerDelegate

class ScrollableShelfView::GradientLayerDelegate : public ui::LayerDelegate {
 public:
  GradientLayerDelegate() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~GradientLayerDelegate() override { layer_.set_delegate(nullptr); }

  bool IsStartFadeZoneVisible() const {
    return !start_fade_zone_.zone_rect.IsEmpty();
  }
  bool IsEndFadeZoneVisible() const {
    return !end_fade_zone_.zone_rect.IsEmpty();
  }

  void set_start_fade_zone(const FadeZone& fade_zone) {
    start_fade_zone_ = fade_zone;
  }
  void set_end_fade_zone(const FadeZone& fade_zone) {
    end_fade_zone_ = fade_zone;
  }
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

    recorder.canvas()->DrawColor(SK_ColorBLACK, SkBlendMode::kSrc);

    if (!start_fade_zone_.zone_rect.IsEmpty())
      DrawFadeZone(start_fade_zone_, recorder.canvas());
    if (!end_fade_zone_.zone_rect.IsEmpty())
      DrawFadeZone(end_fade_zone_, recorder.canvas());
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  void DrawFadeZone(const FadeZone& fade_zone, gfx::Canvas* canvas) {
    gfx::Point start_point;
    gfx::Point end_point;
    if (fade_zone.is_horizontal) {
      start_point = gfx::Point(fade_zone.zone_rect.x(), 0);
      end_point = gfx::Point(fade_zone.zone_rect.right(), 0);
    } else {
      start_point = gfx::Point(0, fade_zone.zone_rect.y());
      end_point = gfx::Point(0, fade_zone.zone_rect.bottom());
    }

    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setAntiAlias(false);

    flags.setShader(gfx::CreateGradientShader(
        start_point, end_point,
        fade_zone.fade_in ? SK_ColorTRANSPARENT : SK_ColorBLACK,
        fade_zone.fade_in ? SK_ColorBLACK : SK_ColorTRANSPARENT));

    canvas->DrawRect(fade_zone.zone_rect, flags);
  }

  ui::Layer layer_;

  FadeZone start_fade_zone_;
  FadeZone end_fade_zone_;

  DISALLOW_COPY_AND_ASSIGN(GradientLayerDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfArrowView

class ScrollableShelfView::ScrollableShelfArrowView
    : public ScrollArrowView,
      public views::ViewTargeterDelegate {
 public:
  explicit ScrollableShelfArrowView(ArrowType arrow_type,
                                    bool is_horizontal_alignment,
                                    Shelf* shelf,
                                    ShelfButtonDelegate* shelf_button_delegate)
      : ScrollArrowView(arrow_type,
                        is_horizontal_alignment,
                        shelf,
                        shelf_button_delegate) {
    SetInkDropMode(InkDropMode::OFF);
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }
  ~ScrollableShelfArrowView() override = default;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    gfx::Rect bounds = gfx::Rect(size());

    // Calculate padding for the tap area. (Should be 32 x shelfButtonSize)
    constexpr int horizontalPadding =
        (kArrowButtonTapAreaHorizontal - kArrowButtonSize) / 2;
    const int verticalPadding =
        (ShelfConfig::Get()->button_size() - kArrowButtonSize) / 2;
    bounds.Inset(gfx::Insets(-verticalPadding, -horizontalPadding));
    return bounds.Intersects(rect);
  }

  // Make ScrollRectToVisible a no-op because ScrollableShelfArrowView is
  // always visible/invisible depending on the layout strategy at fixed
  // locations. So it does not need to be scrolled to show.
  // TODO (andrewxu): Moves all of functions related with scrolling into
  // ScrollableShelfContainerView. Then erase this empty function.
  void ScrollRectToVisible(const gfx::Rect& rect) override {}
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfAnimationMetricsReporter

class ScrollableShelfAnimationMetricsReporter
    : public ui::AnimationMetricsReporter {
 public:
  ScrollableShelfAnimationMetricsReporter() {}

  ~ScrollableShelfAnimationMetricsReporter() override = default;

  // ui::AnimationMetricsReporter:
  void Report(int value) override {
    base::UmaHistogramPercentage(kAnimationSmoothnessHistogram, value);
    if (IsInTabletMode()) {
      if (Shell::Get()->app_list_controller()->IsVisible()) {
        base::UmaHistogramPercentage(
            kAnimationSmoothnessTabletLauncherVisibleHistogram, value);
      } else {
        base::UmaHistogramPercentage(
            kAnimationSmoothnessTabletLauncherHiddenHistogram, value);
      }
    } else {
      if (Shell::Get()->app_list_controller()->IsVisible()) {
        base::UmaHistogramPercentage(
            kAnimationSmoothnessClamshellLauncherVisibleHistogram, value);
      } else {
        base::UmaHistogramPercentage(
            kAnimationSmoothnessClamshellLauncherHiddenHistogram, value);
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfAnimationMetricsReporter);
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfContainerView

class ScrollableShelfContainerView : public ShelfContainerView,
                                     public views::ViewTargeterDelegate {
 public:
  explicit ScrollableShelfContainerView(
      ScrollableShelfView* scrollable_shelf_view)
      : ShelfContainerView(scrollable_shelf_view->shelf_view()),
        scrollable_shelf_view_(scrollable_shelf_view) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }
  ~ScrollableShelfContainerView() override = default;

  // ShelfContainerView:
  void TranslateShelfView(const gfx::Vector2dF& offset) override;

 private:
  // views::View:
  void Layout() override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  ScrollableShelfView* scrollable_shelf_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfContainerView);
};

void ScrollableShelfContainerView::TranslateShelfView(
    const gfx::Vector2dF& offset) {
  ShelfContainerView::TranslateShelfView(
      scrollable_shelf_view_->ShouldAdaptToRTL() ? -offset : offset);
}

void ScrollableShelfContainerView::Layout() {
  // Should not use ShelfView::GetPreferredSize in replace of
  // CalculateIdealSize. Because ShelfView::CalculatePreferredSize relies on the
  // bounds of app icon. Meanwhile, the icon's bounds may be updated by
  // animation.
  const gfx::Rect ideal_bounds = gfx::Rect(CalculateIdealSize());

  const gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect shelf_view_bounds =
      local_bounds.Contains(ideal_bounds) ? local_bounds : ideal_bounds;

  if (shelf_view_->shelf()->IsHorizontalAlignment())
    shelf_view_bounds.set_x(ShelfConfig::Get()->GetAppIconEndPadding());
  else
    shelf_view_bounds.set_y(ShelfConfig::Get()->GetAppIconEndPadding());

  shelf_view_->SetBoundsRect(shelf_view_bounds);
}

bool ScrollableShelfContainerView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  // This view's layer is clipped. So the view should only handle the events
  // within the area after cilp.

  gfx::RectF bounds = gfx::RectF(scrollable_shelf_view_->visible_space());
  views::View::ConvertRectToTarget(scrollable_shelf_view_, this, &bounds);
  return ToEnclosedRect(bounds).Contains(rect);
}

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
    : shelf_view_(new ShelfView(model,
                                shelf,
                                /*drag_and_drop_host=*/this,
                                /*shelf_button_delegate=*/this)),
      page_flip_time_threshold_(kShelfPageFlipDelay),
      animation_metrics_reporter_(
          std::make_unique<ScrollableShelfAnimationMetricsReporter>()) {
  Shell::Get()->AddShellObserver(this);
  set_allow_deactivate_on_esc(true);
}

ScrollableShelfView::~ScrollableShelfView() {
  Shell::Get()->RemoveShellObserver(this);
  GetShelf()->tooltip()->set_shelf_tooltip_delegate(nullptr);
}

void ScrollableShelfView::Init() {
  // Although there is no animation for ScrollableShelfView, a layer is still
  // needed. Otherwise, the child view without its own layer will be painted on
  // RootView and RootView is beneath |opaque_background_| in ShelfWidget. As a
  // result, the child view will not show.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Initialize the shelf container view.
  // Note that |shelf_container_view_| should be under the arrow buttons. It
  // ensures that the arrow button receives the tapping events which happen
  // within the overlapping zone between the arrow button's tapping area and
  // the bounds of |shelf_container_view_|.
  shelf_container_view_ =
      AddChildView(std::make_unique<ScrollableShelfContainerView>(this));
  shelf_container_view_->Initialize();

  // Initialize the left arrow button.
  left_arrow_ = AddChildView(std::make_unique<ScrollableShelfArrowView>(
      ScrollArrowView::kLeft, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  // Initialize the right arrow button.
  right_arrow_ = AddChildView(std::make_unique<ScrollableShelfArrowView>(
      ScrollArrowView::kRight, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  gradient_layer_delegate_ = std::make_unique<GradientLayerDelegate>();
  layer()->SetMaskLayer(gradient_layer_delegate_->layer());

  focus_search_ = std::make_unique<ScrollableShelfFocusSearch>(this);

  GetShelf()->tooltip()->set_shelf_tooltip_delegate(this);

  set_context_menu_controller(this);

  // Initializes |shelf_view_| after scrollable shelf view's children are
  // initialized.
  shelf_view_->Init();
}

void ScrollableShelfView::OnFocusRingActivationChanged(bool activated) {
  if (activated) {
    focus_ring_activated_ = true;
    SetPaneFocusAndFocusDefault();
    if (IsInTabletMode() && chromeos::switches::ShouldShowShelfHotseat())
      GetShelf()->shelf_widget()->ForceToShowHotseat();
  } else {
    // Shows the gradient shader when the focus ring is disabled.
    focus_ring_activated_ = false;
    if (IsInTabletMode() && chromeos::switches::ShouldShowShelfHotseat())
      GetShelf()->shelf_widget()->ForceToHideHotseat();
  }

  MaybeUpdateGradientZone(/*is_left_arrow_changed=*/false,
                          /*is_right_arrow_changed=*/false);
}

void ScrollableShelfView::ScrollToNewPage(bool forward) {
  const float offset = CalculatePageScrollingOffset(forward, layout_strategy_);
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
    ScrollToMainOffset(CalculateScrollUpperBound(GetSpaceForIcons()),
                       /*animating=*/true);
    return FindLastFocusableChild();
  } else {
    ScrollToMainOffset(/*target_offset=*/0.f, /*animating=*/true);
    return FindFirstFocusableChild();
  }
}

gfx::Rect ScrollableShelfView::GetHotseatBackgroundBounds() const {
  return available_space_;
}

bool ScrollableShelfView::ShouldAdaptToRTL() const {
  return base::i18n::IsRTL() && GetShelf()->IsHorizontalAlignment();
}

gfx::Rect ScrollableShelfView::GetTargetScreenBoundsOfItemIcon(
    const ShelfID& id) const {
  DCHECK(GetShelf()->IsHorizontalAlignment());

  // Calculates the available space for child views based on the target bounds.
  const gfx::Insets edge_padding =
      CalculateEdgePadding(/*use_target_bounds=*/true);
  gfx::Rect target_space = GetAvailableLocalBounds(/*use_target_bounds=*/true);
  target_space.Inset(edge_padding);

  const int target_scroll_offset =
      CalculateScrollOffsetForTargetAvailableSpace(target_space);

  gfx::Rect icon_bounds = shelf_view_->view_model()->ideal_bounds(
      shelf_view_->model()->ItemIndexByID(id));

  icon_bounds.Offset(edge_padding.left() - padding_insets_.left(), 0);

  // Transforms |icon_bounds| from shelf view's coordinates to scrollable shelf
  // view's coordinates manually.
  icon_bounds.Offset(
      -target_scroll_offset + ShelfConfig::Get()->GetAppIconEndPadding(), 0);

  // If the icon is invisible under the target view bounds, replaces the actual
  // icon's bounds with the rectangle centering on the edge of |target_space|.
  const gfx::Point icon_bounds_center = icon_bounds.CenterPoint();
  if (icon_bounds_center.x() > target_space.right()) {
    icon_bounds.Offset(target_space.right_center().OffsetFromOrigin() -
                       icon_bounds_center.OffsetFromOrigin());
  } else if (icon_bounds_center.x() < target_space.x()) {
    icon_bounds.Offset(target_space.left_center().OffsetFromOrigin() -
                       icon_bounds_center.OffsetFromOrigin());
  }

  // Hotseat's target bounds may differ from the actual bounds. So it has to
  // transform the bounds manually from view's local coordinates to screen.
  // Notes that the target bounds stored in shelf layout manager are adapted to
  // RTL already while |icon_bounds| are not adjusted to RTL yet.
  gfx::Rect hotseat_bounds_in_screen =
      GetShelf()->shelf_layout_manager()->GetHotseatBounds();
  if (ShouldAdaptToRTL()) {
    // One simple way for transformation under RTL is: (1) Transforms hotseat
    // target bounds from RTL to LTR. (2) Calculates the icon's bounds in screen
    // under LTR. (3) Transforms the icon's bounds to RTL.
    gfx::Rect display_bounds =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(GetWidget()->GetNativeView())
            .bounds();
    hotseat_bounds_in_screen.set_x(display_bounds.right() -
                                   hotseat_bounds_in_screen.right());
    icon_bounds.Offset(hotseat_bounds_in_screen.OffsetFromOrigin());
    icon_bounds.set_x(display_bounds.right() - icon_bounds.right());
  } else {
    icon_bounds.Offset(hotseat_bounds_in_screen.OffsetFromOrigin());
  }

  return icon_bounds;
}

views::View* ScrollableShelfView::GetShelfContainerViewForTest() {
  return shelf_container_view_;
}

void ScrollableShelfView::SetRoundedCornersForShelf(
    bool show,
    views::View* ink_drop_host) {
  shelf_container_view_->layer()->SetRoundedCornerRadius(
      show && InkDropNeedsClipping(ink_drop_host)
          ? CalculateShelfContainerRoundedCorners()
          : gfx::RoundedCornersF());
  shelf_container_view_->layer()->SetIsFastRoundedCorner(true);
}

bool ScrollableShelfView::InkDropNeedsClipping(views::View* ink_drop_host) {
  // The ink drop needs to be clipped only if the host is the app at one of the
  // corners of the shelf. This happens if it is either the first or the last
  // tappable app and no arrow is showing on its side.
  if (shelf_view_->view_model()->view_at(first_tappable_app_index_) ==
      ink_drop_host) {
    return !(layout_strategy_ == kShowButtons ||
             layout_strategy_ == kShowLeftArrowButton);
  }
  if (shelf_view_->view_model()->view_at(last_tappable_app_index_) ==
      ink_drop_host) {
    return !(layout_strategy_ == kShowButtons ||
             layout_strategy_ == kShowRightArrowButton);
  }
  return false;
}

bool ScrollableShelfView::ShouldAdjustForTest() const {
  return CalculateAdjustmentOffset(CalculateMainAxisScrollDistance(),
                                   layout_strategy_, GetSpaceForIcons());
}

void ScrollableShelfView::SetTestObserver(TestObserver* test_observer) {
  DCHECK(!(test_observer && test_observer_));

  test_observer_ = test_observer;
}

int ScrollableShelfView::CalculateScrollUpperBound(
    int available_space_for_icons) const {
  if (layout_strategy_ == kNotShowArrowButtons)
    return 0;

  // Calculate the length of the available space.
  int available_length = available_space_for_icons -
                         2 * ShelfConfig::Get()->GetAppIconEndPadding();

  // Calculate the length of the preferred size.
  const gfx::Size shelf_preferred_size(
      shelf_container_view_->GetPreferredSize());
  const int preferred_length =
      (GetShelf()->IsHorizontalAlignment() ? shelf_preferred_size.width()
                                           : shelf_preferred_size.height());

  return std::max(0, preferred_length - available_length);
}

float ScrollableShelfView::CalculateClampedScrollOffset(
    float scroll,
    int available_space_for_icons) const {
  const float scroll_upper_bound =
      CalculateScrollUpperBound(available_space_for_icons);
  scroll = base::ClampToRange(scroll, 0.0f, scroll_upper_bound);
  return scroll;
}

void ScrollableShelfView::StartShelfScrollAnimation(float scroll_distance) {
  const gfx::Vector2dF scroll_offset_before_update = scroll_offset_;
  UpdateScrollOffset(scroll_distance);

  if (scroll_offset_before_update == scroll_offset_)
    return;

  StopObservingImplicitAnimations();

  during_scroll_animation_ = true;
  MaybeUpdateGradientZone(/*is_left_arrow_changed=*/false,
                          /*is_right_arrow_changed=*/false);

  ui::ScopedLayerAnimationSettings animation_settings(
      shelf_view_->layer()->GetAnimator());
  animation_settings.SetTweenType(gfx::Tween::EASE_OUT);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  animation_settings.SetAnimationMetricsReporter(
      animation_metrics_reporter_.get());
  animation_settings.AddObserver(this);
  shelf_container_view_->TranslateShelfView(scroll_offset_);
}

ScrollableShelfView::LayoutStrategy
ScrollableShelfView::CalculateLayoutStrategy(int scroll_distance_on_main_axis,
                                             int space_for_icons,
                                             bool use_target_bounds) const {
  if (CanFitAllAppsWithoutScrolling(use_target_bounds))
    return kNotShowArrowButtons;

  if (scroll_distance_on_main_axis == 0) {
    // No invisible shelf buttons at the left side. So hide the left button.
    return kShowRightArrowButton;
  }

  if (scroll_distance_on_main_axis ==
      CalculateScrollUpperBound(space_for_icons)) {
    // If there is no invisible shelf button at the right side, hide the right
    // button.
    return kShowLeftArrowButton;
  }

  // There are invisible shelf buttons at both sides. So show two buttons.
  return kShowButtons;
}

bool ScrollableShelfView::ShouldApplyDisplayCentering(
    bool use_target_bounds) const {
  if (!CanFitAllAppsWithoutScrolling(use_target_bounds))
    return false;

  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetWidget()->GetNativeWindow());
  const int display_size_primary = GetShelf()->PrimaryAxisValue(
      display_bounds.width(), display_bounds.height());
  const int status_widget_size =
      GetStatusWidgetSizeOnPrimaryAxis(use_target_bounds);

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
  gfx::Size arrow_button_size(kArrowButtonSize, kArrowButtonSize);
  gfx::Rect shelf_container_bounds = gfx::Rect(size());

  // Transpose and layout as if it is horizontal.
  const bool is_horizontal = GetShelf()->IsHorizontalAlignment();
  if (!is_horizontal)
    shelf_container_bounds.Transpose();

  gfx::Size arrow_button_group_size(kArrowButtonGroupWidth,
                                    shelf_container_bounds.height());

  // The bounds of |left_arrow_| and |right_arrow_| are in the
  // ScrollableShelfView's local coordinates.
  gfx::Rect left_arrow_bounds;
  gfx::Rect right_arrow_bounds;

  const int before_padding =
      is_horizontal ? padding_insets_.left() : padding_insets_.top();
  const int after_padding =
      is_horizontal ? padding_insets_.right() : padding_insets_.bottom();

  // Calculates the bounds of the left arrow button. If the left arrow button
  // should not show, |left_arrow_bounds| should be empty.
  if (layout_strategy_ == kShowLeftArrowButton ||
      layout_strategy_ == kShowButtons) {
    left_arrow_bounds = gfx::Rect(arrow_button_group_size);
    left_arrow_bounds.Offset(before_padding, 0);
    left_arrow_bounds.Inset(kArrowButtonEndPadding, 0, kDistanceToArrowButton,
                            0);
    left_arrow_bounds.ClampToCenteredSize(arrow_button_size);
  }

  if (layout_strategy_ == kShowRightArrowButton ||
      layout_strategy_ == kShowButtons) {
    gfx::Point right_arrow_start_point(
        shelf_container_bounds.right() - after_padding - kArrowButtonGroupWidth,
        0);
    right_arrow_bounds =
        gfx::Rect(right_arrow_start_point, arrow_button_group_size);
    right_arrow_bounds.Inset(kDistanceToArrowButton, 0, kArrowButtonEndPadding,
                             0);
    right_arrow_bounds.ClampToCenteredSize(arrow_button_size);
  }

  // Adjust the bounds when not showing in the horizontal
  // alignment.tShelf()->IsHorizontalAlignment()) {
  if (!is_horizontal) {
    left_arrow_bounds.Transpose();
    right_arrow_bounds.Transpose();
    shelf_container_bounds.Transpose();
  }

  const bool is_left_arrow_changed =
      (left_arrow_->bounds() != left_arrow_bounds) ||
      (!left_arrow_bounds.IsEmpty() && !left_arrow_->GetVisible());
  const bool is_right_arrow_changed =
      (right_arrow_->bounds() != right_arrow_bounds) ||
      (!right_arrow_bounds.IsEmpty() && !right_arrow_->GetVisible());

  // Layout |left_arrow_| if it should show.
  left_arrow_->SetVisible(!left_arrow_bounds.IsEmpty());
  if (left_arrow_->GetVisible())
    left_arrow_->SetBoundsRect(left_arrow_bounds);

  // Layout |right_arrow_| if it should show.
  right_arrow_->SetVisible(!right_arrow_bounds.IsEmpty());
  if (right_arrow_->GetVisible())
    right_arrow_->SetBoundsRect(right_arrow_bounds);

  // Layer::Clone(), which may be triggered by screen rotation, does not copy
  // the mask layer. So we may need to reset the mask layer.
  if (!layer()->layer_mask_layer()) {
    DCHECK(!gradient_layer_delegate_->layer()->layer_mask_back_link());
    layer()->SetMaskLayer(gradient_layer_delegate_->layer());
  }

  if (gradient_layer_delegate_->layer()->bounds() != layer()->bounds())
    gradient_layer_delegate_->layer()->SetBounds(layer()->bounds());

  MaybeUpdateGradientZone(is_left_arrow_changed, is_right_arrow_changed);

  // Layout |shelf_container_view_|.
  shelf_container_view_->SetBoundsRect(shelf_container_bounds);
}

void ScrollableShelfView::ChildPreferredSizeChanged(views::View* child) {
  // Add/remove a shelf icon may change the layout strategy.
  Layout();
}

void ScrollableShelfView::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->finger_count() != 2)
    return;
  if (ShouldDelegateScrollToShelf(*event)) {
    ui::MouseWheelEvent wheel(*event);
    GetShelf()->ProcessMouseWheelEvent(&wheel);
    event->StopPropagation();
  }
}

void ScrollableShelfView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    HandleMouseWheelEvent(event->AsMouseWheelEvent());
    return;
  }

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
  if (ShouldHandleGestures(*event) && ProcessGestureEvent(*event)) {
    // |event| is consumed by ScrollableShelfView.
    event->SetHandled();
  } else if (shelf_view_->HandleGestureEvent(event)) {
    // |event| is consumed by ShelfView.
    event->StopPropagation();
  } else if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    // |event| is consumed by neither ScrollableShelfView nor ShelfView. So the
    // gesture end event will not be propagated to this view. Then we need to
    // reset the class members related with scroll status explicitly.
    ResetScrollStatus();
  }
}

void ScrollableShelfView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  GetViewAccessibility().OverrideNextFocus(GetShelf()->GetStatusAreaWidget());
  GetViewAccessibility().OverridePreviousFocus(
      GetShelf()->shelf_widget()->navigation_widget());
}

void ScrollableShelfView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  const gfx::Vector2dF old_scroll_offset = scroll_offset_;

  // The changed view bounds may lead to update on the available space.
  UpdateAvailableSpaceAndScroll();

  // When AdjustOffset() returns true, shelf view is scrolled by animation.
  if (!AdjustOffset() && old_scroll_offset != scroll_offset_)
    shelf_container_view_->TranslateShelfView(scroll_offset_);
}

void ScrollableShelfView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent != shelf_view_)
    return;

  shelf_view_->UpdateVisibleIndices();

  const gfx::Vector2dF old_scroll_offset = scroll_offset_;

  // Adding/removing an icon may change the padding then affect the available
  // space.
  UpdateAvailableSpaceAndScroll();

  if (old_scroll_offset != scroll_offset_)
    shelf_container_view_->TranslateShelfView(scroll_offset_);
}

void ScrollableShelfView::ScrollRectToVisible(const gfx::Rect& rect) {
  // Transform |rect| to local view coordinates taking |scroll_offset_| into
  // consideration.
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  gfx::Rect rect_after_adjustment = rect;
  if (is_horizontal_alignment)
    rect_after_adjustment.Offset(-scroll_offset_.x(), 0);
  else
    rect_after_adjustment.Offset(0, -scroll_offset_.y());

  // |rect_after_adjustment| is already shown completely. So scroll is not
  // needed.
  if (visible_space_.Contains(rect_after_adjustment)) {
    AdjustOffset();
    return;
  }

  const int original_offset = CalculateMainAxisScrollDistance();

  // |forward| indicates the scroll direction.
  const bool forward =
      is_horizontal_alignment
          ? rect_after_adjustment.right() > visible_space_.right()
          : rect_after_adjustment.bottom() > visible_space_.bottom();

  // Scrolling |shelf_view_| has the following side-effects:
  // (1) May change the layout strategy.
  // (2) May change the visible space.
  // (3) Must change the scrolling offset.
  // (4) Must change |rect_after_adjustment|'s coordinates after adjusting the
  // scroll.
  LayoutStrategy layout_strategy_after_scroll = layout_strategy_;
  int main_axis_offset_after_scroll = original_offset;
  gfx::Rect visible_space_after_scroll = visible_space_;
  gfx::Rect rect_after_scroll = rect_after_adjustment;

  // In each iteration, it scrolls |shelf_view_| to the neighboring page.
  // Terminating the loop iteration if:
  // (1) Find the suitable page which shows |rect| completely.
  // (2) Cannot scroll |shelf_view_| anymore (it may happen with ChromeVox
  // enabled).
  while (!visible_space_after_scroll.Contains(rect_after_scroll)) {
    int page_scroll_distance =
        CalculatePageScrollingOffset(forward, layout_strategy_after_scroll);

    // Breaking the while loop if it cannot scroll anymore.
    if (!page_scroll_distance)
      break;

    main_axis_offset_after_scroll += page_scroll_distance;
    main_axis_offset_after_scroll = CalculateClampedScrollOffset(
        main_axis_offset_after_scroll, GetSpaceForIcons());
    layout_strategy_after_scroll = CalculateLayoutStrategy(
        main_axis_offset_after_scroll, GetSpaceForIcons(),
        /*use_target_bounds= =*/false);
    main_axis_offset_after_scroll = CalculateScrollDistanceAfterAdjustment(
        main_axis_offset_after_scroll, layout_strategy_after_scroll);
    visible_space_after_scroll =
        CalculateVisibleSpace(layout_strategy_after_scroll);

    rect_after_scroll = rect_after_adjustment;
    const int offset_delta = main_axis_offset_after_scroll - original_offset;
    if (is_horizontal_alignment)
      rect_after_scroll.Offset(-offset_delta, 0);
    else
      rect_after_scroll.Offset(0, -offset_delta);
  }

  if (!visible_space_after_scroll.Contains(rect_after_scroll))
    return;

  ScrollToMainOffset(main_axis_offset_after_scroll, /*animating=*/true);
}

std::unique_ptr<ui::Layer> ScrollableShelfView::RecreateLayer() {
  layer()->SetMaskLayer(nullptr);
  return views::View::RecreateLayer();
}

const char* ScrollableShelfView::GetClassName() const {
  return "ScrollableShelfView";
}

void ScrollableShelfView::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  if ((button == left_arrow_) || (button == right_arrow_))
    return;

  shelf_view_->OnShelfButtonAboutToRequestFocusFromTabTraversal(button,
                                                                reverse);
  ShelfWidget* shelf_widget = GetShelf()->shelf_widget();
  // In tablet mode, when the hotseat is not extended but one of the buttons
  // gets focused, it should update the visibility of the hotseat.
  if (IsInTabletMode() && chromeos::switches::ShouldShowShelfHotseat() &&
      !shelf_widget->hotseat_widget()->IsExtended()) {
    shelf_widget->shelf_layout_manager()->UpdateVisibilityState();
  }
}

void ScrollableShelfView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event,
                                        views::InkDrop* ink_drop) {
  if ((sender == left_arrow_) || (sender == right_arrow_)) {
    ScrollToNewPage(sender == right_arrow_);
    return;
  }

  shelf_view_->ButtonPressed(sender, event, ink_drop);
}

void ScrollableShelfView::HandleAccessibleActionScrollToMakeVisible(
    ShelfButton* button) {
  if (IsInTabletMode() && chromeos::switches::ShouldShowShelfHotseat()) {
    // Only in tablet mode with hotseat enabled, may scrollable shelf be hidden.
    GetShelf()->shelf_widget()->ForceToShowHotseat();
  }
}

void ScrollableShelfView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |point| is in screen coordinates. So it does not need to transform.
  shelf_view_->ShowContextMenuForViewImpl(shelf_view_, point, source_type);
}

void ScrollableShelfView::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  left_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  right_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  scroll_offset_ = gfx::Vector2dF();
  ScrollToMainOffset(CalculateMainAxisScrollDistance(), /*animating=*/false);
  Layout();
}

bool ScrollableShelfView::ShouldShowTooltipForView(
    const views::View* view) const {
  if (!view || !view->parent())
    return false;

  if (view == left_arrow_ || view == right_arrow_)
    return true;

  if (view->parent() != shelf_view_)
    return false;

  // The shelf item corresponding to |view| may have been removed from the
  // model.
  if (!shelf_view_->ShouldShowTooltipForChildView(view))
    return false;

  const gfx::Rect screen_bounds = view->GetBoundsInScreen();
  gfx::Rect visible_bounds_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_bounds_in_screen);

  return visible_bounds_in_screen.Contains(screen_bounds);
}

bool ScrollableShelfView::ShouldHideTooltip(
    const gfx::Point& cursor_location) const {
  return !visible_space_.Contains(cursor_location);
}

const std::vector<aura::Window*> ScrollableShelfView::GetOpenWindowsForView(
    views::View* view) {
  if (!view || view->parent() != shelf_view_)
    return std::vector<aura::Window*>();

  return shelf_view_->GetOpenWindowsForView(view);
}

base::string16 ScrollableShelfView::GetTitleForView(
    const views::View* view) const {
  if (!view || !view->parent())
    return base::string16();

  if (view->parent() == shelf_view_)
    return shelf_view_->GetTitleForView(view);

  if (view == left_arrow_)
    return l10n_util::GetStringUTF16(IDS_SHELF_PREVIOUS);

  if (view == right_arrow_)
    return l10n_util::GetStringUTF16(IDS_SHELF_NEXT);

  return base::string16();
}

views::View* ScrollableShelfView::GetViewForEvent(const ui::Event& event) {
  if (event.target() == GetWidget()->GetNativeWindow())
    return this;

  return nullptr;
}

void ScrollableShelfView::CreateDragIconProxyByLocationWithNoAnimation(
    const gfx::Point& origin_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    float scale_factor,
    int blur_radius) {
  drag_icon_ = std::make_unique<DragImageView>(
      GetWidget()->GetNativeWindow()->GetRootWindow(),
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  drag_icon_->SetImage(icon);
  const gfx::Rect replaced_view_screen_bounds =
      replaced_view->GetBoundsInScreen();
  drag_icon_->SetBoundsInScreen(replaced_view_screen_bounds);
  drag_icon_->GetWidget()->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  drag_icon_->SetWidgetVisible(true);
  drag_icon_->SetPaintToLayer();
  drag_icon_->layer()->SetFillsBoundsOpaquely(false);
}

void ScrollableShelfView::UpdateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates) {
  drag_icon_->SetScreenPosition(location_in_screen_coordinates);

  if (IsDragIconWithinVisibleSpace()) {
    page_flip_timer_.AbandonAndStop();
    return;
  }

  if (!page_flip_timer_.IsRunning()) {
    page_flip_timer_.Start(FROM_HERE, page_flip_time_threshold_, this,
                           &ScrollableShelfView::OnPageFlipTimer);
  }
}

void ScrollableShelfView::DestroyDragIconProxy() {
  if (page_flip_timer_.IsRunning())
    page_flip_timer_.AbandonAndStop();

  ShelfAppButton* drag_view = shelf_view_->drag_view();

  const bool should_start_animation =
      drag_view && !shelf_view_->dragged_off_shelf() && drag_icon_.get();
  if (!should_start_animation) {
    drag_icon_.reset();
    return;
  }

  // The ideal bounds stored in view model are in |shelf_view_|'s coordinates.
  views::ViewModel* shelf_view_model = shelf_view_->view_model();
  const gfx::Rect target_bounds = shelf_view_model->ideal_bounds(
      shelf_view_model->GetIndexOfView(drag_view));
  const gfx::Rect mirrored_target_bounds_in_shelf_view =
      shelf_view_->GetMirroredRect(target_bounds);

  // No animation is created if the target slot for the drag icon is not on the
  // current page. This edge case may be triggered by trying to move the icon of
  // a running app to the area exclusively for pinned apps.
  gfx::RectF target_bounds_in_local(mirrored_target_bounds_in_shelf_view);
  ConvertRectToTarget(shelf_view_, this, &target_bounds_in_local);
  if (!visible_space_.Contains(gfx::ToEnclosedRect(target_bounds_in_local))) {
    drag_icon_.reset();
    drag_view->layer()->SetOpacity(1.0f);
    return;
  }

  const bool drag_originated_from_app_list =
      chromeos::switches::ShouldShowScrollableShelf() &&
      shelf_view_->IsShelfViewHandlingDragAndDrop();

  // The drag proxy is the DragImageView created for the DragAndDropHost which
  // could be either the ShelfView or ScrollableShelfView depending on where the
  // drag originated from.
  std::unique_ptr<DragImageView> drag_proxy =
      drag_originated_from_app_list
          ? std::unique_ptr<DragImageView>(
                shelf_view_->RetrieveDragIconProxyAndClearDragProxyState())
          : std::move(drag_icon_);
  gfx::RectF start_in_screen(drag_proxy->GetBoundsInScreen());

  gfx::RectF end_in_screen;
  if (drag_originated_from_app_list) {
    // If the drag originated from the AppList, |drag_view| bounds are no longer
    // correct, so calculate the ideal bounds of the dragged icon.
    end_in_screen = gfx::RectF(
        GetTargetScreenBoundsOfItemIcon(shelf_view_->drag_and_drop_shelf_id()));
  } else {
    end_in_screen = gfx::RectF(drag_view->GetIconBoundsInScreen());
  }

  if (start_in_screen.IsEmpty() || end_in_screen.IsEmpty()) {
    drag_proxy.reset();
    return;
  }

  drag_icon_drop_animation_delegate_ =
      std::make_unique<DragIconDropAnimationDelegate>(
          drag_view, start_in_screen, end_in_screen, std::move(drag_proxy));
  drag_icon_drop_animation_delegate_->StartAnimation();
}

bool ScrollableShelfView::StartDrag(
    const std::string& app_id,
    const gfx::Point& location_in_screen_coordinates) {
  return false;
}

bool ScrollableShelfView::Drag(
    const gfx::Point& location_in_screen_coordinates) {
  return false;
}

void ScrollableShelfView::OnImplicitAnimationsCompleted() {
  during_scroll_animation_ = false;
  Layout();

  // Notifies ChromeVox of the changed location at the end of animation.
  shelf_view_->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                        /*send_native_event=*/true);

  if (!drag_icon_.get())
    return;

  if (IsDragIconWithinVisibleSpace())
    return;

  page_flip_timer_.Start(FROM_HERE, page_flip_time_threshold_, this,
                         &ScrollableShelfView::OnPageFlipTimer);
}

bool ScrollableShelfView::ShouldShowLeftArrow() const {
  return (layout_strategy_ == kShowLeftArrowButton) ||
         (layout_strategy_ == kShowButtons);
}

bool ScrollableShelfView::ShouldShowRightArrow() const {
  return (layout_strategy_ == kShowRightArrowButton) ||
         (layout_strategy_ == kShowButtons);
}

gfx::Insets ScrollableShelfView::CalculateEdgePadding(
    bool use_target_bounds) const {
  // Display centering alignment
  if (ShouldApplyDisplayCentering(use_target_bounds))
    return CalculatePaddingForDisplayCentering(use_target_bounds);

  const int icons_size = shelf_view_->GetSizeOfAppIcons(
                             shelf_view_->number_of_visible_apps(), false) +
                         2 * ShelfConfig::Get()->GetAppIconEndPadding();
  const int base_padding = ShelfConfig::Get()->app_icon_group_margin();

  const gfx::Rect available_local_bounds =
      GetAvailableLocalBounds(use_target_bounds);
  const int available_size_for_app_icons =
      GetShelf()->PrimaryAxisValue(available_local_bounds.width(),
                                   available_local_bounds.height()) -
      2 * ShelfConfig::Get()->app_icon_group_margin();

  int gap = CanFitAllAppsWithoutScrolling(use_target_bounds)
                ? available_size_for_app_icons - icons_size  // shelf centering
                : 0;                                         // overflow

  // Calculates the paddings before/after the visible area of scrollable shelf.
  const int before_padding = base_padding + gap / 2;
  const int after_padding = base_padding + (gap % 2 ? gap / 2 + 1 : gap / 2);

  gfx::Insets padding_insets;
  if (GetShelf()->IsHorizontalAlignment()) {
    padding_insets =
        gfx::Insets(/*top=*/0, before_padding, /*bottom=*/0, after_padding);
  } else {
    padding_insets =
        gfx::Insets(before_padding, /*left=*/0, after_padding, /*right=*/0);
  }

  return padding_insets;
}

int ScrollableShelfView::GetStatusWidgetSizeOnPrimaryAxis(
    bool use_target_bounds) const {
  const gfx::Size status_widget_size =
      use_target_bounds
          ? GetShelf()->shelf_layout_manager()->GetStatusAreaBounds().size()
          : GetShelf()
                ->shelf_widget()
                ->status_area_widget()
                ->GetWindowBoundsInScreen()
                .size();
  return GetShelf()->PrimaryAxisValue(status_widget_size.width(),
                                      status_widget_size.height());
}

gfx::Rect ScrollableShelfView::GetAvailableLocalBounds(
    bool use_target_bounds) const {
  return use_target_bounds ? gfx::Rect(GetShelf()
                                           ->shelf_layout_manager()
                                           ->GetHotseatBounds()
                                           .size())
                           : GetLocalBounds();
}

gfx::Insets ScrollableShelfView::CalculatePaddingForDisplayCentering(
    bool use_target_bounds) const {
  const int icons_size = shelf_view_->GetSizeOfAppIcons(
                             shelf_view_->number_of_visible_apps(), false) +
                         2 * ShelfConfig::Get()->GetAppIconEndPadding();
  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetWidget()->GetNativeWindow());
  const int display_size_primary = GetShelf()->PrimaryAxisValue(
      display_bounds.width(), display_bounds.height());
  const int gap = (display_size_primary - icons_size) / 2;

  // Calculates paddings in view coordinates.
  const gfx::Rect screen_bounds =
      use_target_bounds ? GetShelf()->shelf_layout_manager()->GetHotseatBounds()
                        : GetBoundsInScreen();
  const int before_padding = gap - GetShelf()->PrimaryAxisValue(
                                       screen_bounds.x() - display_bounds.x(),
                                       screen_bounds.y() - display_bounds.y());
  const int after_padding =
      gap - GetShelf()->PrimaryAxisValue(
                display_bounds.right() - screen_bounds.right(),
                display_bounds.bottom() - screen_bounds.bottom());

  gfx::Insets padding_insets;
  if (GetShelf()->IsHorizontalAlignment()) {
    padding_insets =
        gfx::Insets(/*top=*/0, before_padding, /*bottom=*/0, after_padding);
  } else {
    padding_insets =
        gfx::Insets(before_padding, /*left=*/0, after_padding, /*right=*/0);
  }

  return padding_insets;
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

    // The change in |scroll_status_| may lead to update on the gradient zone.
    MaybeUpdateGradientZone(/*is_left_arrow_changed=*/false,
                            /*is_right_arrow_changed=*/false);
  }

  if (event.type() == ui::ET_GESTURE_END)
    ResetScrollStatus();

  return should_handle_gestures;
}

void ScrollableShelfView::ResetScrollStatus() {
  scroll_status_ = kNotInScroll;
  scroll_offset_before_main_axis_scrolling_ = gfx::Vector2dF();
  layout_strategy_before_main_axis_scrolling_ = kNotShowArrowButtons;

  // The change in |scroll_status_| may lead to update on the gradient zone.
  MaybeUpdateGradientZone(/*is_left_arrow_changed=*/false,
                          /*is_right_arrow_changed=*/false);
}

bool ScrollableShelfView::ProcessGestureEvent(const ui::GestureEvent& event) {
  if (layout_strategy_ == kNotShowArrowButtons)
    return true;

  // Handle scroll-related events, but don't do anything special for begin and
  // end.
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    DCHECK(!presentation_time_recorder_);
    if (IsInTabletMode()) {
      if (Shell::Get()->app_list_controller()->IsVisible()) {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            kScrollDraggingTabletLauncherVisibleHistogram,
            kScrollDraggingTabletLauncherVisibleMaxLatencyHistogram);
      } else {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            kScrollDraggingTabletLauncherHiddenHistogram,
            kScrollDraggingTabletLauncherHiddenMaxLatencyHistogram);
      }
    } else {
      if (Shell::Get()->app_list_controller()->IsVisible()) {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            kScrollDraggingClamshellLauncherVisibleHistogram,
            kScrollDraggingClamshellLauncherVisibleMaxLatencyHistogram);
      } else {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            kScrollDraggingClamshellLauncherHiddenHistogram,
            kScrollDraggingClamshellLauncherHiddenMaxLatencyHistogram);
      }
    }
    return true;
  }
  if (event.type() == ui::ET_GESTURE_SCROLL_END) {
    presentation_time_recorder_.reset();
    return true;
  }

  if (event.type() == ui::ET_GESTURE_END) {
    // The type of scrolling offset is float to ensure that ScrollableShelfView
    // is responsive to slow gesture scrolling. However, after offset
    // adjustment, the scrolling offset should be floored.
    scroll_offset_ = gfx::ToFlooredVector2d(scroll_offset_);

    AdjustOffset();
    return true;
  }

  if (event.type() == ui::ET_SCROLL_FLING_START) {
    const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
    if (!ShouldHandleScroll(gfx::Vector2dF(event.details().velocity_x(),
                                           event.details().velocity_y()),
                            /*is_gesture_fling=*/true)) {
      return false;
    }

    int scroll_velocity = is_horizontal_alignment
                              ? event.details().velocity_x()
                              : event.details().velocity_y();
    if (ShouldAdaptToRTL())
      scroll_velocity = -scroll_velocity;
    float page_scrolling_offset = CalculatePageScrollingOffset(
        scroll_velocity < 0, layout_strategy_before_main_axis_scrolling_);

    // Only starts animation when scroll distance is greater than zero.
    if (std::fabs(page_scrolling_offset) > 0.f) {
      ScrollToMainOffset((is_horizontal_alignment
                              ? scroll_offset_before_main_axis_scrolling_.x()
                              : scroll_offset_before_main_axis_scrolling_.y()) +
                             page_scrolling_offset,
                         /*animating=*/true);
    }

    return true;
  }

  if (event.type() != ui::ET_GESTURE_SCROLL_UPDATE)
    return false;

  DCHECK(presentation_time_recorder_);
  presentation_time_recorder_->RequestNext();

  const int scroll_x = -event.details().scroll_x();
  if (GetShelf()->IsHorizontalAlignment()) {
    ScrollByXOffset(ShouldAdaptToRTL() ? -scroll_x : scroll_x,
                    /*animate=*/false);
  } else {
    ScrollByYOffset(-event.details().scroll_y(), /*animate=*/false);
  }
  return true;
}

void ScrollableShelfView::HandleMouseWheelEvent(ui::MouseWheelEvent* event) {
  // Note that the scrolling from touchpad is propagated as mouse wheel event.

  // When shelf is horizontally aligned, the mouse wheel event may be handled
  // by AppList.
  if (!ShouldHandleScroll(gfx::Vector2dF(event->x_offset(), event->y_offset()),
                          /*is_gesture_fling=*/false) &&
      GetShelf()->IsHorizontalAlignment()) {
    GetShelf()->ProcessMouseWheelEvent(event);
    return;
  }

  event->SetHandled();

  // Scrolling the mouse wheel may create multiple mouse wheel events at the
  // same time. If the scrollable shelf view is during scrolling animation at
  // this moment, do not handle the mouse wheel event.
  if (shelf_view_->layer()->GetAnimator()->is_animating())
    return;

  if (GetShelf()->IsHorizontalAlignment()) {
    const float x_offset = event->x_offset();
    const float y_offset = event->y_offset();
    // If the shelf is bottom aligned, we can scroll over the shelf contents if
    // the scroll is horizontal or vertical (in the case of a mousewheel
    // scroll). We take the biggest offset difference of the vertical and
    // horizontal components to determine the offset to scroll over the
    // contents.
    float max_absolute_offset =
        abs(x_offset) > abs(y_offset) ? x_offset : y_offset;
    ScrollByXOffset(
        CalculatePageScrollingOffset(max_absolute_offset < 0, layout_strategy_),
        /*animating=*/true);
  } else {
    ScrollByYOffset(
        CalculatePageScrollingOffset(event->y_offset() < 0, layout_strategy_),
        /*animating=*/true);
  }
}

void ScrollableShelfView::ScrollByXOffset(float x_offset, bool animating) {
  ScrollToMainOffset(scroll_offset_.x() + x_offset, animating);
}

void ScrollableShelfView::ScrollByYOffset(float y_offset, bool animating) {
  ScrollToMainOffset(scroll_offset_.y() + y_offset, animating);
}

void ScrollableShelfView::ScrollToMainOffset(float target_offset,
                                             bool animating) {
  if (animating) {
    StartShelfScrollAnimation(target_offset);
  } else {
    UpdateScrollOffset(target_offset);
    shelf_container_view_->TranslateShelfView(scroll_offset_);
  }
}

float ScrollableShelfView::CalculatePageScrollingOffset(
    bool forward,
    LayoutStrategy layout_strategy) const {
  // Returns zero if inputs are invalid.
  const bool invalid = (layout_strategy == kNotShowArrowButtons) ||
                       (layout_strategy == kShowLeftArrowButton && forward) ||
                       (layout_strategy == kShowRightArrowButton && !forward);
  if (invalid)
    return 0;

  // Implement the arrow button handler in the same way with the gesture
  // scrolling. The key is to calculate the suitable scroll distance.

  float offset;

  // The available space for icons excluding the area taken by arrow button(s).
  int space_excluding_arrow;

  if (layout_strategy == kShowRightArrowButton) {
    space_excluding_arrow = GetSpaceForIcons() - kArrowButtonGroupWidth;

    // After scrolling, the left arrow button will show. Adapts the offset
    // to the extra arrow button.
    const int offset_for_extra_arrow =
        kArrowButtonGroupWidth - ShelfConfig::Get()->GetAppIconEndPadding();

    const int mod = space_excluding_arrow % GetUnit();
    offset = space_excluding_arrow - mod - offset_for_extra_arrow;
  } else if (layout_strategy == kShowButtons ||
             layout_strategy == kShowLeftArrowButton) {
    space_excluding_arrow = GetSpaceForIcons() - 2 * kArrowButtonGroupWidth;
    const int mod = space_excluding_arrow % GetUnit();
    offset = space_excluding_arrow - mod;

    // Layout of kShowLeftArrowButton can be regarded as the layout of
    // kShowButtons with extra offset.
    if (layout_strategy == kShowLeftArrowButton) {
      const int extra_offset =
          -ShelfConfig::Get()->button_spacing() -
          (GetSpaceForIcons() - kArrowButtonGroupWidth) % GetUnit() +
          ShelfConfig::Get()->GetAppIconEndPadding();
      offset += extra_offset;
    }
  }

  DCHECK_GT(offset, 0);

  if (!forward)
    offset = -offset;

  return offset;
}

void ScrollableShelfView::UpdateGradientZone() {
  gradient_layer_delegate_->set_start_fade_zone(CalculateStartGradientZone());
  gradient_layer_delegate_->set_end_fade_zone(CalculateEndGradientZone());

  SchedulePaint();
}

ScrollableShelfView::FadeZone ScrollableShelfView::CalculateStartGradientZone()
    const {
  if (!should_show_start_gradient_zone_)
    return FadeZone();

  gfx::Rect zone_rect;
  bool fade_in = false;
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  gfx::Rect left_arrow_bounds;
  if (left_arrow_->GetVisible()) {
    left_arrow_bounds = left_arrow_->bounds();
  } else {
    // If the arrow button is invisible, the gradient zone is created on
    // the side of |visible_space_|.
    left_arrow_bounds = gfx::Rect(
        is_horizontal_alignment ? GetMirroredRect(visible_space_).x() : 0,
        is_horizontal_alignment ? 0 : visible_space_.y(), /*width=*/0,
        /*height=*/0);
  }

  if (is_horizontal_alignment) {
    int gradient_start;
    int gradient_end;

    // Calculates the bounds of the gradient zone. Enlarge the gradient zone by
    // one-pixel to offset the potential rounding error during rendering (we
    // also do it in CalculateEndGradientZone()).
    if (ShouldAdaptToRTL()) {
      const gfx::Rect mirrored_left_arrow_bounds =
          GetMirroredRect(left_arrow_bounds);
      gradient_start = mirrored_left_arrow_bounds.x() - kGradientZoneLength;
      gradient_end = mirrored_left_arrow_bounds.x() + 1;
    } else {
      gradient_start = left_arrow_bounds.right() - 1;
      gradient_end = left_arrow_bounds.right() + kGradientZoneLength;
    }
    zone_rect =
        gfx::Rect(gradient_start, 0, gradient_end - gradient_start, height());
  } else {
    zone_rect = gfx::Rect(0, left_arrow_bounds.bottom() - 1, width(),
                          kGradientZoneLength + 1);
  }

  fade_in = !ShouldAdaptToRTL();

  return {zone_rect, fade_in, is_horizontal_alignment};
}

ScrollableShelfView::FadeZone ScrollableShelfView::CalculateEndGradientZone()
    const {
  if (!should_show_end_gradient_zone_)
    return FadeZone();

  gfx::Rect zone_rect;
  bool fade_in = false;
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();

  gfx::Rect right_arrow_bounds;
  if (right_arrow_->GetVisible()) {
    right_arrow_bounds = right_arrow_->bounds();
  } else {
    // If the arrow button is invisible, the gradient zone is created on
    // the side of |visible_space_|.
    right_arrow_bounds = gfx::Rect(
        is_horizontal_alignment ? GetMirroredRect(visible_space_).right() : 0,
        is_horizontal_alignment ? 0 : visible_space_.bottom(),
        /*width=*/0,
        /*height=*/0);
  }

  if (is_horizontal_alignment) {
    int gradient_start;
    int gradient_end;

    if (ShouldAdaptToRTL()) {
      const gfx::Rect mirrored_right_arrow_bounds =
          GetMirroredRect(right_arrow_bounds);
      gradient_start = mirrored_right_arrow_bounds.right() - 1;
      gradient_end = mirrored_right_arrow_bounds.right() + kGradientZoneLength;
    } else {
      gradient_start = right_arrow_bounds.x() - kGradientZoneLength;
      gradient_end = right_arrow_bounds.x() + 1;
    }
    zone_rect =
        gfx::Rect(gradient_start, 0, gradient_end - gradient_start, height());
  } else {
    zone_rect = gfx::Rect(0, right_arrow_bounds.y() - kGradientZoneLength,
                          width(), kGradientZoneLength + 1);
  }

  fade_in = ShouldAdaptToRTL();

  return {zone_rect, fade_in, is_horizontal_alignment};
}

void ScrollableShelfView::UpdateGradientZoneState() {
  // The gradient zone is not painted when the focus ring shows in order to
  // display the focus ring correctly.
  if (focus_ring_activated_) {
    should_show_start_gradient_zone_ = false;
    should_show_end_gradient_zone_ = false;
    return;
  }

  if (during_scroll_animation_) {
    should_show_start_gradient_zone_ = true;
    should_show_end_gradient_zone_ = true;
    return;
  }

  should_show_start_gradient_zone_ = layout_strategy_ == kShowLeftArrowButton ||
                                     (layout_strategy_ == kShowButtons &&
                                      scroll_status_ == kAlongMainAxisScroll);
  should_show_end_gradient_zone_ = ShouldShowRightArrow();
}

void ScrollableShelfView::MaybeUpdateGradientZone(bool is_left_arrow_changed,
                                                  bool is_right_arrow_changed) {
  // Fade zones should be updated if:
  // (1) Fade zone's visibility changes.
  // (2) Fade zone should show and the arrow button's location changes.
  UpdateGradientZoneState();
  const bool should_update_end_fade_zone =
      (should_show_end_gradient_zone_ !=
       gradient_layer_delegate_->IsEndFadeZoneVisible()) ||
      (should_show_end_gradient_zone_ && is_right_arrow_changed);
  const bool should_update_start_fade_zone =
      (should_show_start_gradient_zone_ !=
       gradient_layer_delegate_->IsStartFadeZoneVisible()) ||
      (should_show_start_gradient_zone_ && is_left_arrow_changed);

  if (should_update_start_fade_zone || should_update_end_fade_zone)
    UpdateGradientZone();
}

int ScrollableShelfView::GetActualScrollOffset(
    int main_axis_scroll_distance,
    LayoutStrategy layout_strategy) const {
  return (layout_strategy == kShowButtons ||
          layout_strategy == kShowLeftArrowButton)
             ? (main_axis_scroll_distance + kArrowButtonGroupWidth -
                ShelfConfig::Get()->GetAppIconEndPadding())
             : main_axis_scroll_distance;
}

void ScrollableShelfView::UpdateTappableIconIndices() {
  const std::pair<int, int> tappable_indices = CalculateTappableIconIndices(
      layout_strategy_, CalculateMainAxisScrollDistance());
  first_tappable_app_index_ = tappable_indices.first;
  last_tappable_app_index_ = tappable_indices.second;
}

std::pair<int, int> ScrollableShelfView::CalculateTappableIconIndices(
    ScrollableShelfView::LayoutStrategy layout_strategy,
    int scroll_distance_on_main_axis) const {
  std::pair<int, int> tappable_icon_indices;

  int& first_tappable_app_index = tappable_icon_indices.first;
  int& last_tappable_app_index = tappable_icon_indices.second;

  if (layout_strategy == ScrollableShelfView::kNotShowArrowButtons) {
    first_tappable_app_index = shelf_view_->first_visible_index();
    last_tappable_app_index = shelf_view_->last_visible_index();
    return tappable_icon_indices;
  }

  const int visible_size = GetShelf()->IsHorizontalAlignment()
                               ? visible_space_.width()
                               : visible_space_.height();

  if (layout_strategy == kShowRightArrowButton ||
      layout_strategy == kShowButtons) {
    first_tappable_app_index = scroll_distance_on_main_axis / GetUnit() +
                               (layout_strategy == kShowButtons ? 1 : 0);
    last_tappable_app_index =
        first_tappable_app_index + visible_size / GetUnit();

    const int end_of_last_tappable_app = last_tappable_app_index * GetUnit() +
                                         ShelfConfig::Get()->button_size() -
                                         scroll_distance_on_main_axis;
    if (end_of_last_tappable_app > visible_size)
      last_tappable_app_index--;
  } else {
    DCHECK_EQ(layout_strategy, kShowLeftArrowButton);
    last_tappable_app_index = shelf_view_->last_visible_index();
    first_tappable_app_index =
        last_tappable_app_index - visible_size / GetUnit() + 1;
  }

  return tappable_icon_indices;
}

views::View* ScrollableShelfView::FindFirstFocusableChild() {
  if (shelf_view_->view_model()->view_size() == 0)
    return nullptr;

  return shelf_view_->view_model()->view_at(shelf_view_->first_visible_index());
}

views::View* ScrollableShelfView::FindLastFocusableChild() {
  if (shelf_view_->view_model()->view_size() == 0)
    return nullptr;

  return shelf_view_->view_model()->view_at(shelf_view_->last_visible_index());
}

int ScrollableShelfView::GetSpaceForIcons() const {
  return GetShelf()->IsHorizontalAlignment() ? available_space_.width()
                                             : available_space_.height();
}

bool ScrollableShelfView::CanFitAllAppsWithoutScrolling(
    bool use_target_bounds) const {
  const gfx::Rect available_rect = GetAvailableLocalBounds(use_target_bounds);
  const int available_length =
      (GetShelf()->IsHorizontalAlignment() ? available_rect.width()
                                           : available_rect.height()) -
      2 * ShelfConfig::Get()->app_icon_group_margin();

  gfx::Size preferred_size = GetPreferredSize();
  int preferred_length = GetShelf()->IsHorizontalAlignment()
                             ? preferred_size.width()
                             : preferred_size.height();
  preferred_length += 2 * ShelfConfig::Get()->GetAppIconEndPadding();

  return available_length >= preferred_length;
}

bool ScrollableShelfView::ShouldHandleScroll(const gfx::Vector2dF& offset,
                                             bool is_gesture_scrolling) const {
  // When the shelf is aligned at the bottom, a horizontal mousewheel scroll may
  // also be handled by the ScrollableShelf if the offset along the main axis is
  // 0. This case is mainly triggered by an event generated in the MouseWheel,
  // but not in the touchpad, as touchpads events are caught on ScrollEvent.
  // If there is an x component to the scroll, consider this instead of the y
  // axis because the horizontal scroll could move the scrollable shelf.
  const float main_axis_offset =
      GetShelf()->IsHorizontalAlignment() && offset.x() != 0 ? offset.x()
                                                             : offset.y();

  const int threshold = is_gesture_scrolling ? kGestureFlingVelocityThreshold
                                             : KScrollOffsetThreshold;
  return abs(main_axis_offset) > threshold;
}

bool ScrollableShelfView::AdjustOffset() {
  const int offset = CalculateAdjustmentOffset(
      CalculateMainAxisScrollDistance(), layout_strategy_, GetSpaceForIcons());

  // Returns early when it does not need to adjust the shelf view's location.
  if (!offset)
    return false;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, /*animate=*/true);
  else
    ScrollByYOffset(offset, /*animate=*/true);

  return true;
}

int ScrollableShelfView::CalculateAdjustmentOffset(
    int main_axis_scroll_distance,
    LayoutStrategy layout_strategy,
    int available_space_for_icons) const {
  // Returns early when it does not need to adjust the shelf view's location.
  if (layout_strategy == kNotShowArrowButtons ||
      main_axis_scroll_distance >=
          CalculateScrollUpperBound(available_space_for_icons)) {
    return 0;
  }

  const int remainder =
      GetActualScrollOffset(main_axis_scroll_distance, layout_strategy) %
      GetUnit();
  int offset = remainder > GetGestureDragThreshold() ? GetUnit() - remainder
                                                     : -remainder;

  return offset;
}

int ScrollableShelfView::CalculateScrollDistanceAfterAdjustment(
    int main_axis_scroll_distance,
    LayoutStrategy layout_strategy) const {
  return main_axis_scroll_distance +
         CalculateAdjustmentOffset(main_axis_scroll_distance, layout_strategy,
                                   GetSpaceForIcons());
}

void ScrollableShelfView::UpdateAvailableSpace() {
  padding_insets_ = CalculateEdgePadding(/*use_target_bounds=*/false);
  available_space_ = GetLocalBounds();
  available_space_.Inset(padding_insets_);

  // The hotseat uses |available_space_| to determine where to show its
  // background, so notify it when it is recalculated.
  if (HotseatWidget::ShouldShowHotseatBackground()) {
    GetShelf()->shelf_widget()->hotseat_widget()->SetOpaqueBackground(
        GetHotseatBackgroundBounds());
  }

  // Paddings are within the shelf view. It makes sure that |shelf_view_|'s
  // bounds are not changed by adding/removing the shelf icon under the same
  // layout strategy.
  const int horizontal_inset =
      ShouldAdaptToRTL() ? padding_insets_.right() : padding_insets_.left();
  shelf_view_->set_app_icons_layout_offset(GetShelf()->IsHorizontalAlignment()
                                               ? horizontal_inset
                                               : padding_insets_.top());
}

gfx::Rect ScrollableShelfView::CalculateVisibleSpace(
    LayoutStrategy layout_strategy) const {
  const bool in_hotseat_tablet =
      chromeos::switches::ShouldShowShelfHotseat() && IsInTabletMode();
  if (layout_strategy == kNotShowArrowButtons && !in_hotseat_tablet)
    return GetAvailableLocalBounds(/*use_target_bounds=*/false);

  const bool should_show_left_arrow =
      (layout_strategy == kShowLeftArrowButton) ||
      (layout_strategy == kShowButtons);
  const bool should_show_right_arrow =
      (layout_strategy == kShowRightArrowButton) ||
      (layout_strategy == kShowButtons);

  const int before_padding =
      (should_show_left_arrow ? kArrowButtonGroupWidth : 0);
  const int after_padding =
      (should_show_right_arrow ? kArrowButtonGroupWidth : 0);

  gfx::Insets visible_space_insets;
  if (ShouldAdaptToRTL()) {
    visible_space_insets = gfx::Insets(0, after_padding, 0, before_padding);
  } else {
    visible_space_insets =
        GetShelf()->IsHorizontalAlignment()
            ? gfx::Insets(0, before_padding, 0, after_padding)
            : gfx::Insets(before_padding, 0, after_padding, 0);
  }
  visible_space_insets -= CalculateRipplePaddingInsets();

  gfx::Rect visible_space = available_space_;
  visible_space.Inset(visible_space_insets);

  return visible_space;
}

gfx::Insets ScrollableShelfView::CalculateRipplePaddingInsets() const {
  // Indicates whether it is in tablet mode with hotseat enabled.
  const bool in_hotseat_tablet =
      chromeos::switches::ShouldShowShelfHotseat() && IsInTabletMode();

  const int ripple_padding =
      ShelfConfig::Get()->scrollable_shelf_ripple_padding();
  const int before_padding =
      (in_hotseat_tablet && !ShouldShowLeftArrow()) ? 0 : ripple_padding;
  const int after_padding =
      (in_hotseat_tablet && !ShouldShowRightArrow()) ? 0 : ripple_padding;

  if (ShouldAdaptToRTL())
    return gfx::Insets(0, after_padding, 0, before_padding);

  return GetShelf()->IsHorizontalAlignment()
             ? gfx::Insets(0, before_padding, 0, after_padding)
             : gfx::Insets(before_padding, 0, after_padding, 0);
}

gfx::RoundedCornersF
ScrollableShelfView::CalculateShelfContainerRoundedCorners() const {
  if (!chromeos::switches::ShouldShowShelfHotseat() || !IsInTabletMode())
    return gfx::RoundedCornersF();

  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  const float radius = (is_horizontal_alignment ? height() : width()) / 2.f;

  int upper_left = left_arrow_->GetVisible() ? 0 : radius;

  int upper_right;
  if (is_horizontal_alignment)
    upper_right = right_arrow_->GetVisible() ? 0 : radius;
  else
    upper_right = left_arrow_->GetVisible() ? 0 : radius;

  int lower_right = right_arrow_->GetVisible() ? 0 : radius;

  int lower_left;
  if (is_horizontal_alignment)
    lower_left = left_arrow_->GetVisible() ? 0 : radius;
  else
    lower_left = right_arrow_->GetVisible() ? 0 : radius;

  if (ShouldAdaptToRTL()) {
    std::swap(upper_left, upper_right);
    std::swap(lower_left, lower_right);
  }

  return gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left);
}

void ScrollableShelfView::OnPageFlipTimer() {
  gfx::Rect visible_space_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_space_in_screen);

  const gfx::Rect drag_icon_screen_bounds = drag_icon_->GetBoundsInScreen();

  DCHECK(!visible_space_in_screen.Contains(drag_icon_screen_bounds));

  // Calculates the page scrolling direction based on the |drag_icon_|'s
  // location and the bounds of the visible space.
  bool should_scroll_to_next;
  if (ShouldAdaptToRTL()) {
    should_scroll_to_next =
        drag_icon_screen_bounds.x() < visible_space_in_screen.x();
  } else {
    should_scroll_to_next =
        GetShelf()->IsHorizontalAlignment()
            ? drag_icon_screen_bounds.right() > visible_space_in_screen.right()
            : drag_icon_screen_bounds.bottom() >
                  visible_space_in_screen.bottom();
  }

  ScrollToNewPage(/*forward=*/should_scroll_to_next);

  if (test_observer_)
    test_observer_->OnPageFlipTimerFired();
}

bool ScrollableShelfView::IsDragIconWithinVisibleSpace() const {
  gfx::Rect visible_space_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_space_in_screen);

  const gfx::Rect drag_icon_screen_bounds = drag_icon_->GetBoundsInScreen();

  if (GetShelf()->IsHorizontalAlignment()) {
    return drag_icon_screen_bounds.x() >= visible_space_in_screen.x() &&
           drag_icon_screen_bounds.right() <= visible_space_in_screen.right();
  }

  return drag_icon_screen_bounds.y() >= visible_space_in_screen.y() &&
         drag_icon_screen_bounds.bottom() <= visible_space_in_screen.bottom();
}

bool ScrollableShelfView::ShouldDelegateScrollToShelf(
    const ui::ScrollEvent& event) const {
  // When the shelf is not aligned in the bottom, the events should be
  // propagated and handled as MouseWheel events.
  if (!GetShelf()->IsHorizontalAlignment())
    return false;

  if (event.type() != ui::ET_SCROLL)
    return false;

  const float main_offset = event.x_offset();
  const float cross_offset = event.y_offset();
  // We only delegate to the shelf scroll events across the main axis,
  // otherwise, let them propagate and be handled as MouseWheel Events.
  return std::abs(main_offset) < std::abs(cross_offset);
}

float ScrollableShelfView::CalculateMainAxisScrollDistance() const {
  return GetShelf()->IsHorizontalAlignment() ? scroll_offset_.x()
                                             : scroll_offset_.y();
}

void ScrollableShelfView::UpdateScrollOffset(float target_offset) {
  target_offset =
      CalculateClampedScrollOffset(target_offset, GetSpaceForIcons());

  if (GetShelf()->IsHorizontalAlignment())
    scroll_offset_.set_x(target_offset);
  else
    scroll_offset_.set_y(target_offset);

  // Calculating the layout strategy relies on |scroll_offset_|.
  LayoutStrategy new_strategy = CalculateLayoutStrategy(
      CalculateMainAxisScrollDistance(), GetSpaceForIcons(),
      /*use_target_bounds=*/false);

  const bool strategy_needs_update = (layout_strategy_ != new_strategy);
  if (strategy_needs_update) {
    layout_strategy_ = new_strategy;
    InvalidateLayout();
  }

  visible_space_ = CalculateVisibleSpace(layout_strategy_);
  shelf_container_view_->layer()->SetClipRect(visible_space_);

  UpdateTappableIconIndices();
}

void ScrollableShelfView::UpdateAvailableSpaceAndScroll() {
  UpdateAvailableSpace();
  UpdateScrollOffset(CalculateMainAxisScrollDistance());
}

int ScrollableShelfView::CalculateScrollOffsetForTargetAvailableSpace(
    const gfx::Rect& target_space) const {
  // Ensures that the scroll offset is legal under the updated available space.
  const int available_space_for_icons =
      GetShelf()->PrimaryAxisValue(target_space.width(), target_space.height());
  int target_scroll_offset = CalculateClampedScrollOffset(
      CalculateMainAxisScrollDistance(), available_space_for_icons);

  // Calculates the layout strategy based on the new scroll offset.
  LayoutStrategy new_strategy =
      CalculateLayoutStrategy(target_scroll_offset, available_space_for_icons,
                              /*use_target_bounds=*/true);

  // Adjusts the scroll offset with the new strategy.
  target_scroll_offset += CalculateAdjustmentOffset(
      target_scroll_offset, new_strategy, available_space_for_icons);

  return target_scroll_offset;
}

}  // namespace ash
