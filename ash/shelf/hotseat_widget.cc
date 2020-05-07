// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"
#include <memory>
#include <utility>

#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/animation_metrics_reporter.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

void DoScopedAnimationSetting(
    ui::ScopedLayerAnimationSettings* animation_setter,
    ui::AnimationMetricsReporter* metrics_reporter) {
  animation_setter->SetTransitionDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  animation_setter->SetTweenType(gfx::Tween::EASE_OUT);
  animation_setter->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (metrics_reporter) {
    animation_setter->SetAnimationMetricsReporter(metrics_reporter);
  }
}

// Custom window targeter for the hotseat. Used so the hotseat only processes
// events that land on the visible portion of the hotseat, and only while the
// hotseat is not animating.
class HotseatWindowTargeter : public aura::WindowTargeter {
 public:
  explicit HotseatWindowTargeter(HotseatWidget* hotseat_widget)
      : hotseat_widget_(hotseat_widget) {}
  ~HotseatWindowTargeter() override = default;

  HotseatWindowTargeter(const HotseatWindowTargeter& other) = delete;
  HotseatWindowTargeter& operator=(const HotseatWindowTargeter& rhs) = delete;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    // Do not handle events if the hotseat window is animating as it may animate
    // over other items which want to process events.
    if (hotseat_widget_->GetLayer()->GetAnimator()->is_animating())
      return false;
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (target == hotseat_widget_->GetNativeWindow()) {
      // Shrink the hit bounds from the size of the window to the size of the
      // hotseat translucent background.
      gfx::Rect hit_bounds = target->bounds();
      hit_bounds.ClampToCenteredSize(
          hotseat_widget_->GetTranslucentBackgroundSize());
      *hit_test_rect_mouse = *hit_test_rect_touch = hit_bounds;
      return true;
    }
    return aura::WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                                 hit_test_rect_touch);
  }

 private:
  // Unowned and guaranteed to be not null for the duration of |this|.
  HotseatWidget* const hotseat_widget_;
};

}  // namespace

class HotseatWidget::DelegateView : public HotseatTransitionAnimator::Observer,
                                    public views::WidgetDelegateView,
                                    public OverviewObserver,
                                    public WallpaperControllerObserver {
 public:
  DelegateView() : translucent_background_(ui::LAYER_SOLID_COLOR) {
    translucent_background_.SetName("hotseat/Background");
  }
  ~DelegateView() override;

  // Initializes the view.
  void Init(ScrollableShelfView* scrollable_shelf_view,
            ui::Layer* parent_layer,
            HotseatWidget* hotseat_widget);

  // Updates the hotseat background.
  void UpdateTranslucentBackground();

  void SetTranslucentBackground(const gfx::Rect& translucent_background_bounds);

  // Sets whether the background should be blurred as requested by the argument,
  // unless the feature flag is disabled or |disable_blur_for_animations_| is
  // true, in which case this disables background blur.
  void SetBackgroundBlur(bool enable_blur);

  // HotseatTransitionAnimator::Observer:
  void OnHotseatTransitionAnimationWillStart(HotseatState from_state,
                                             HotseatState to_state) override;
  void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                         HotseatState to_state) override;
  void OnHotseatTransitionAnimationAborted() override;

  // views::WidgetDelegateView:
  bool CanActivate() const override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  int background_blur() const {
    return translucent_background_.background_blur();
  }

  bool is_translucent_background_visible_for_test() {
    return translucent_background_.GetTargetVisibility();
  }

 private:
  void SetParentLayer(ui::Layer* layer);

  FocusCycler* focus_cycler_ = nullptr;
  // A background layer that may be visible depending on HotseatState.
  ui::Layer translucent_background_;
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;  // unowned.
  HotseatWidget* hotseat_widget_ = nullptr;               // unowned.
  // Blur is disabled during animations to improve performance.
  int blur_lock_ = 0;

  // The most recent color that the |translucent_background_| has been animated
  // to.
  SkColor target_color_ = SK_ColorTRANSPARENT;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

HotseatWidget::DelegateView::~DelegateView() {
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (wallpaper_controller)
    wallpaper_controller->RemoveObserver(this);
  if (overview_controller)
    overview_controller->RemoveObserver(this);
}

void HotseatWidget::DelegateView::Init(
    ScrollableShelfView* scrollable_shelf_view,
    ui::Layer* parent_layer,
    HotseatWidget* hotseat_widget) {
  hotseat_widget_ = hotseat_widget;
  SetLayoutManager(std::make_unique<views::FillLayout>());

  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (wallpaper_controller)
    wallpaper_controller->AddObserver(this);
  if (overview_controller) {
    overview_controller->AddObserver(this);
    if (overview_controller->InOverviewSession())
      ++blur_lock_;
  }
  SetParentLayer(parent_layer);

  DCHECK(scrollable_shelf_view);
  scrollable_shelf_view_ = scrollable_shelf_view;
}

void HotseatWidget::DelegateView::UpdateTranslucentBackground() {
  if (!HotseatWidget::ShouldShowHotseatBackground()) {
    translucent_background_.SetVisible(false);
    SetBackgroundBlur(false);
    return;
  }

  SetTranslucentBackground(
      scrollable_shelf_view_->GetHotseatBackgroundBounds());
}

void HotseatWidget::DelegateView::SetTranslucentBackground(
    const gfx::Rect& background_bounds) {
  DCHECK(HotseatWidget::ShouldShowHotseatBackground());

  translucent_background_.SetVisible(true);
  SetBackgroundBlur(/*enable_blur=*/true);
  ui::AnimationMetricsReporter* metrics_reporter =
      hotseat_widget_
          ? hotseat_widget_->GetTranslucentBackgroundMetricsReporter()
          : nullptr;

  if (ShelfConfig::Get()->GetDefaultShelfColor() != target_color_) {
    ui::ScopedLayerAnimationSettings color_animation_setter(
        translucent_background_.GetAnimator());
    DoScopedAnimationSetting(&color_animation_setter, metrics_reporter);
    target_color_ = ShelfConfig::Get()->GetDefaultShelfColor();
    translucent_background_.SetColor(target_color_);
  }

  // Animate the bounds change if there's a change of width (for instance when
  // dragging an app into, or out of, the shelf) and meanwhile scrollable
  // shelf's bounds does not update at the same time.
  const bool animate_bounds =
      background_bounds.width() != translucent_background_.bounds().width() &&
      (scrollable_shelf_view_ &&
       !scrollable_shelf_view_->NeedUpdateToTargetBounds());
  base::Optional<ui::ScopedLayerAnimationSettings> bounds_animation_setter;
  if (animate_bounds) {
    bounds_animation_setter.emplace(translucent_background_.GetAnimator());
    DoScopedAnimationSetting(&bounds_animation_setter.value(),
                             metrics_reporter);
  }

  const int radius = hotseat_widget_->GetHotseatSize() / 2;
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  if (translucent_background_.rounded_corner_radii() != rounded_corners)
    translucent_background_.SetRoundedCornerRadius(rounded_corners);

  if (translucent_background_.bounds() != background_bounds)
    translucent_background_.SetBounds(background_bounds);
}

void HotseatWidget::DelegateView::SetBackgroundBlur(bool enable_blur) {
  if (!features::IsBackgroundBlurEnabled() || blur_lock_ > 0)
    return;

  const int blur_radius =
      enable_blur ? ShelfConfig::Get()->shelf_blur_radius() : 0;
  if (translucent_background_.background_blur() != blur_radius)
    translucent_background_.SetBackgroundBlur(blur_radius);
}

void HotseatWidget::DelegateView::OnHotseatTransitionAnimationWillStart(
    HotseatState from_state,
    HotseatState to_state) {
  DCHECK_LE(blur_lock_, 2);

  SetBackgroundBlur(false);
  ++blur_lock_;
}

void HotseatWidget::DelegateView::OnHotseatTransitionAnimationEnded(
    HotseatState from_state,
    HotseatState to_state) {
  DCHECK_GT(blur_lock_, 0);

  --blur_lock_;
  SetBackgroundBlur(true);
}

void HotseatWidget::DelegateView::OnHotseatTransitionAnimationAborted() {
  DCHECK_GT(blur_lock_, 0);

  --blur_lock_;
}

bool HotseatWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
}

void HotseatWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&translucent_background_);
}

void HotseatWidget::DelegateView::OnOverviewModeWillStart() {
  DCHECK_LE(blur_lock_, 2);

  SetBackgroundBlur(false);
  ++blur_lock_;
}

void HotseatWidget::DelegateView::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  DCHECK_GT(blur_lock_, 0);

  --blur_lock_;
  SetBackgroundBlur(true);
}

void HotseatWidget::DelegateView::OnWallpaperColorsChanged() {
  UpdateTranslucentBackground();
}

void HotseatWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(&translucent_background_);
  ReorderLayers();
}

HotseatWidget::HotseatWidget() : delegate_view_(new DelegateView()) {
  ShelfConfig::Get()->AddObserver(this);
}

HotseatWidget::~HotseatWidget() {
  ShelfConfig::Get()->RemoveObserver(this);
  shelf_->shelf_widget()->hotseat_transition_animator()->RemoveObserver(
      delegate_view_);
}

bool HotseatWidget::ShouldShowHotseatBackground() {
  return chromeos::switches::ShouldShowShelfHotseat() &&
         Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

void HotseatWidget::Initialize(aura::Window* container, Shelf* shelf) {
  DCHECK(container);
  DCHECK(shelf);
  shelf_ = shelf;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "HotseatWidget";
  params.delegate = delegate_view_;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  Init(std::move(params));
  set_focus_on_creation(false);
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);

  scrollable_shelf_view_ = GetContentsView()->AddChildView(
      std::make_unique<ScrollableShelfView>(ShelfModel::Get(), shelf));
  delegate_view_->Init(scrollable_shelf_view(), GetLayer(), this);

  // The initialization of scrollable shelf should update the translucent
  // background which is stored in |delegate_view_|. So initializes
  // |scrollable_shelf_view_| after |delegate_view_|.
  scrollable_shelf_view_->Init();
}

void HotseatWidget::OnHotseatTransitionAnimatorCreated(
    HotseatTransitionAnimator* animator) {
  shelf_->shelf_widget()->hotseat_transition_animator()->AddObserver(
      delegate_view_);
}

void HotseatWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  views::Widget::OnMouseEvent(event);
}

void HotseatWidget::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();

  if (!event->handled())
    views::Widget::OnGestureEvent(event);
}

bool HotseatWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;

  scrollable_shelf_view_->OnFocusRingActivationChanged(active);
  return true;
}

void HotseatWidget::OnShelfConfigUpdated() {
  set_manually_extended(false);
}

bool HotseatWidget::IsExtended() const {
  DCHECK(GetShelfView()->shelf()->IsHorizontalAlignment());
  const int extended_bottom =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetShelfView()->GetWidget()->GetNativeView())
          .bounds()
          .bottom() -
      (ShelfConfig::Get()->shelf_size() +
       ShelfConfig::Get()->hotseat_bottom_padding());
  return GetWindowBoundsInScreen().bottom() == extended_bottom;
}

void HotseatWidget::FocusFirstOrLastFocusableChild(bool last) {
  GetShelfView()->FindFirstOrLastFocusableChild(last)->RequestFocus();
}

void HotseatWidget::OnTabletModeChanged() {
  GetShelfView()->OnTabletModeChanged();
}

float HotseatWidget::CalculateOpacity() const {
  const float target_opacity =
      GetShelfView()->shelf()->shelf_layout_manager()->GetOpacity();
  return (state() == HotseatState::kExtended) ? 1.0f  // fully translucent
                                              : target_opacity;
}

void HotseatWidget::SetTranslucentBackground(
    const gfx::Rect& translucent_background_bounds) {
  delegate_view_->SetTranslucentBackground(translucent_background_bounds);
}

int HotseatWidget::CalculateHotseatYInScreen(
    HotseatState hotseat_target_state) const {
  DCHECK(shelf_->IsHorizontalAlignment());
  int hotseat_distance_from_bottom_of_display;
  const int hotseat_size = GetHotseatSize();
  switch (hotseat_target_state) {
    case HotseatState::kShownClamshell:
      hotseat_distance_from_bottom_of_display = hotseat_size;
      break;
    case HotseatState::kShownHomeLauncher:
      // When the hotseat state is HotseatState::kShownHomeLauncher, the home
      // launcher is showing in tablet mode. Elevate the hotseat a few px to
      // match the navigation and status area.
      hotseat_distance_from_bottom_of_display =
          hotseat_size + ShelfConfig::Get()->hotseat_bottom_padding();
      break;
    case HotseatState::kHidden:
      // Show the hotseat offscreen.
      hotseat_distance_from_bottom_of_display = 0;
      break;
    case HotseatState::kExtended:
      // Show the hotseat at its extended position.
      hotseat_distance_from_bottom_of_display =
          ShelfConfig::Get()->in_app_shelf_size() +
          ShelfConfig::Get()->hotseat_bottom_padding() + hotseat_size;
      break;
  }
  const int target_shelf_size =
      shelf_->shelf_widget()->GetTargetBounds().size().height();
  const int hotseat_y_in_shelf =
      -(hotseat_distance_from_bottom_of_display - target_shelf_size);
  const int shelf_y = shelf_->shelf_widget()->GetTargetBounds().y();
  return hotseat_y_in_shelf + shelf_y;
}

void HotseatWidget::CalculateTargetBounds() {
  ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  const HotseatState hotseat_target_state =
      layout_manager->CalculateHotseatState(layout_manager->visibility_state(),
                                            layout_manager->auto_hide_state());
  const gfx::Size status_size =
      shelf_->status_area_widget()->GetTargetBounds().size();
  const gfx::Rect shelf_bounds = shelf_->shelf_widget()->GetTargetBounds();
  const int horizontal_edge_spacing =
      ShelfConfig::Get()->control_button_edge_spacing(
          shelf_->IsHorizontalAlignment());
  const int vertical_edge_spacing =
      ShelfConfig::Get()->control_button_edge_spacing(
          !shelf_->IsHorizontalAlignment());
  gfx::Rect nav_bounds = shelf_->navigation_widget()->GetTargetBounds();
  gfx::Point hotseat_origin;
  int hotseat_width;
  int hotseat_height;
  if (shelf_->IsHorizontalAlignment()) {
    hotseat_width = shelf_bounds.width() - nav_bounds.size().width() -
                    horizontal_edge_spacing -
                    ShelfConfig::Get()->app_icon_group_margin() -
                    status_size.width();
    int hotseat_x =
        base::i18n::IsRTL()
            ? nav_bounds.x() - horizontal_edge_spacing - hotseat_width
            : nav_bounds.right() + horizontal_edge_spacing;
    if (hotseat_target_state != HotseatState::kShownHomeLauncher &&
        hotseat_target_state != HotseatState::kShownClamshell) {
      // Give the hotseat more space if it is shown outside of the shelf.
      hotseat_width = shelf_bounds.width();
      hotseat_x = shelf_bounds.x();
    }
    hotseat_origin =
        gfx::Point(hotseat_x, CalculateHotseatYInScreen(hotseat_target_state));
    hotseat_height = GetHotseatSize();
  } else {
    hotseat_origin = gfx::Point(shelf_bounds.x(),
                                nav_bounds.bottom() + vertical_edge_spacing);
    hotseat_width = shelf_bounds.width();
    hotseat_height = shelf_bounds.height() - nav_bounds.size().height() -
                     vertical_edge_spacing -
                     ShelfConfig::Get()->app_icon_group_margin() -
                     status_size.height();
  }
  target_bounds_ =
      gfx::Rect(hotseat_origin, gfx::Size(hotseat_width, hotseat_height));
}

gfx::Rect HotseatWidget::GetTargetBounds() const {
  return target_bounds_;
}

void HotseatWidget::UpdateLayout(bool animate) {
  const LayoutInputs new_layout_inputs = GetLayoutInputs();
  if (layout_inputs_ == new_layout_inputs)
    return;

  // Never show this widget outside of an active session.
  if (!new_layout_inputs.is_active_session_state)
    Hide();

  ui::Layer* layer = GetNativeView()->layer();
  {
    ui::ScopedLayerAnimationSettings animation_setter(layer->GetAnimator());
    animation_setter.SetTransitionDuration(
        animate ? ShelfConfig::Get()->shelf_animation_duration()
                : base::TimeDelta::FromMilliseconds(0));
    animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
    animation_setter.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_setter.SetAnimationMetricsReporter(
        shelf_->GetHotseatTransitionMetricsReporter(state_));

    layer->SetOpacity(new_layout_inputs.opacity);
    SetBounds(new_layout_inputs.bounds);
    layout_inputs_ = new_layout_inputs;
    delegate_view_->UpdateTranslucentBackground();
  }

  // Setting visibility during an animation causes the visibility property to
  // animate. Set the visibility property without an animation.
  if (new_layout_inputs.opacity && new_layout_inputs.is_active_session_state) {
    ShowInactive();
  }
}

void HotseatWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (shelf_->IsHorizontalAlignment())
    target_bounds_.set_y(shelf_position);
  else
    target_bounds_.set_x(shelf_position);
}

gfx::Size HotseatWidget::GetTranslucentBackgroundSize() const {
  DCHECK(scrollable_shelf_view_);
  return scrollable_shelf_view_->GetHotseatBackgroundBounds().size();
}

void HotseatWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

ShelfView* HotseatWidget::GetShelfView() {
  DCHECK(scrollable_shelf_view_);
  return scrollable_shelf_view_->shelf_view();
}

int HotseatWidget::GetHotseatSize() const {
  return ShelfConfig::Get()->GetShelfButtonSize(is_forced_dense_);
}

int HotseatWidget::GetHotseatFullDragAmount() const {
  ShelfConfig* shelf_config = ShelfConfig::Get();
  return shelf_config->shelf_size() + shelf_config->hotseat_bottom_padding() +
         GetHotseatSize();
}

int HotseatWidget::GetHotseatBackgroundBlurForTest() const {
  return delegate_view_->background_blur();
}

bool HotseatWidget::GetIsTranslucentBackgroundVisibleForTest() const {
  return delegate_view_->is_translucent_background_visible_for_test();
}

bool HotseatWidget::IsShowingShelfMenu() const {
  return GetShelfView()->IsShowingMenu();
}

const ShelfView* HotseatWidget::GetShelfView() const {
  return const_cast<const ShelfView*>(
      const_cast<HotseatWidget*>(this)->GetShelfView());
}

ui::AnimationMetricsReporter*
HotseatWidget::GetTranslucentBackgroundMetricsReporter() {
  return shelf_->GetTranslucentBackgroundMetricsReporter(state_);
}

void HotseatWidget::SetState(HotseatState state) {
  if (state_ == state)
    return;

  state_ = state;

  // If the hotseat is not extended we can use the normal targeting as the
  // hidden parts of the hotseat will not block non-shelf items from taking
  if (state == HotseatState::kExtended) {
    hotseat_window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
        GetNativeWindow(), std::make_unique<HotseatWindowTargeter>(this));
  } else {
    hotseat_window_targeter_.reset();
  }
}

HotseatWidget::LayoutInputs HotseatWidget::GetLayoutInputs() const {
  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  return {target_bounds_, CalculateOpacity(),
          layout_manager->is_active_session_state()};
}

}  // namespace ash
