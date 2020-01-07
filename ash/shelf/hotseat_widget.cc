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
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

bool IsScrollableShelfEnabled() {
  return chromeos::switches::ShouldShowScrollableShelf();
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
      // hotseat opaque background.
      gfx::Rect hit_bounds = target->bounds();
      hit_bounds.ClampToCenteredSize(
          hotseat_widget_->GetOpaqueBackgroundSize());
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

class HotseatWidget::DelegateView : public views::WidgetDelegateView,
                                    public WallpaperControllerObserver {
 public:
  explicit DelegateView(WallpaperControllerImpl* wallpaper_controller)
      : opaque_background_(ui::LAYER_SOLID_COLOR),
        wallpaper_controller_(wallpaper_controller) {
    opaque_background_.SetName("hotseat/Background");
  }
  ~DelegateView() override;

  // Initializes the view.
  void Init(ScrollableShelfView* scrollable_shelf_view,
            ui::Layer* parent_layer);

  // Updates the hotseat background.
  void UpdateOpaqueBackground();

  void SetOpaqueBackground(const gfx::Rect& opaque_background_bounds);

  // Updates the hotseat background when tablet mode changes.
  void OnTabletModeChanged();

  // views::WidgetDelegateView:
  bool CanActivate() const override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

 private:
  void SetParentLayer(ui::Layer* layer);

  FocusCycler* focus_cycler_ = nullptr;
  // A background layer that may be visible depending on HotseatState.
  ui::Layer opaque_background_;
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;  // unowned.
  // The WallpaperController, responsible for providing proper colors.
  WallpaperControllerImpl* wallpaper_controller_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

HotseatWidget::DelegateView::~DelegateView() {
  if (wallpaper_controller_)
    wallpaper_controller_->RemoveObserver(this);
}

void HotseatWidget::DelegateView::Init(
    ScrollableShelfView* scrollable_shelf_view,
    ui::Layer* parent_layer) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  if (!chromeos::switches::ShouldShowScrollableShelf())
    return;

  if (wallpaper_controller_)
    wallpaper_controller_->AddObserver(this);
  SetParentLayer(parent_layer);

  DCHECK(scrollable_shelf_view);
  scrollable_shelf_view_ = scrollable_shelf_view;
  UpdateOpaqueBackground();
}

void HotseatWidget::DelegateView::UpdateOpaqueBackground() {
  if (!HotseatWidget::ShouldShowHotseatBackground()) {
    opaque_background_.SetVisible(false);
    if (features::IsBackgroundBlurEnabled())
      opaque_background_.SetBackgroundBlur(0);
    return;
  }

  SetOpaqueBackground(scrollable_shelf_view_->GetHotseatBackgroundBounds());
}

void HotseatWidget::DelegateView::SetOpaqueBackground(
    const gfx::Rect& background_bounds) {
  DCHECK(HotseatWidget::ShouldShowHotseatBackground());

  opaque_background_.SetVisible(true);
  opaque_background_.SetColor(ShelfConfig::Get()->GetDefaultShelfColor());

  const int radius = ShelfConfig::Get()->hotseat_size() / 2;
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  if (opaque_background_.rounded_corner_radii() != rounded_corners)
    opaque_background_.SetRoundedCornerRadius(rounded_corners);

  if (opaque_background_.bounds() != background_bounds)
    opaque_background_.SetBounds(background_bounds);

  if (features::IsBackgroundBlurEnabled()) {
    opaque_background_.SetBackgroundBlur(
        ShelfConfig::Get()->shelf_blur_radius());
  }
}

void HotseatWidget::DelegateView::OnTabletModeChanged() {
  UpdateOpaqueBackground();
}

bool HotseatWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
}

void HotseatWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  if (!chromeos::switches::ShouldShowScrollableShelf())
    return;

  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
}

void HotseatWidget::DelegateView::OnWallpaperColorsChanged() {
  UpdateOpaqueBackground();
}

void HotseatWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  ReorderLayers();
}

HotseatWidget::HotseatWidget()
    : delegate_view_(new DelegateView(Shell::Get()->wallpaper_controller())) {
  ShelfConfig::Get()->AddObserver(this);
}

HotseatWidget::~HotseatWidget() {
  ShelfConfig::Get()->RemoveObserver(this);
}

bool HotseatWidget::ShouldShowHotseatBackground() {
  return chromeos::switches::ShouldShowShelfHotseat() &&
         Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

void HotseatWidget::Initialize(aura::Window* container, Shelf* shelf) {
  DCHECK(container);
  DCHECK(shelf);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "HotseatWidget";
  params.delegate = delegate_view_;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  Init(std::move(params));
  set_focus_on_creation(false);
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);

  if (IsScrollableShelfEnabled()) {
    scrollable_shelf_view_ = GetContentsView()->AddChildView(
        std::make_unique<ScrollableShelfView>(ShelfModel::Get(), shelf));
    scrollable_shelf_view_->Init();
  } else {
    // The shelf view observes the shelf model and creates icons as items are
    // added to the model.
    shelf_view_ = GetContentsView()->AddChildView(std::make_unique<ShelfView>(
        ShelfModel::Get(), shelf, /*drag_and_drop_host=*/nullptr,
        /*shelf_button_delegate=*/nullptr));
    shelf_view_->Init();
  }
  delegate_view_->Init(scrollable_shelf_view(), GetLayer());
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

  if (IsScrollableShelfEnabled())
    scrollable_shelf_view_->OnFocusRingActivationChanged(active);
  else if (active)
    GetShelfView()->SetPaneFocusAndFocusDefault();

  return true;
}

void HotseatWidget::OnShelfConfigUpdated() {
  set_manually_extended(false);
}

bool HotseatWidget::IsShowingOverflowBubble() const {
  return GetShelfView()->IsShowingOverflowBubble();
}

bool HotseatWidget::IsExtended() const {
  DCHECK(GetShelfView()->shelf()->IsHorizontalAlignment());
  const int extended_y =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetShelfView()->GetWidget()->GetNativeView())
          .bounds()
          .bottom() -
      (ShelfConfig::Get()->shelf_size() +
       ShelfConfig::Get()->hotseat_bottom_padding() +
       ShelfConfig::Get()->hotseat_size());
  return GetWindowBoundsInScreen().y() == extended_y;
}

void HotseatWidget::FocusOverflowShelf(bool last_element) {
  if (!IsShowingOverflowBubble())
    return;
  Shell::Get()->focus_cycler()->FocusWidget(
      GetShelfView()->overflow_bubble()->bubble_view()->GetWidget());
  views::View* to_focus =
      GetShelfView()->overflow_shelf()->FindFirstOrLastFocusableChild(
          last_element);
  to_focus->RequestFocus();
}

void HotseatWidget::FocusFirstOrLastFocusableChild(bool last) {
  GetShelfView()->FindFirstOrLastFocusableChild(last)->RequestFocus();
}

void HotseatWidget::OnTabletModeChanged() {
  GetShelfView()->OnTabletModeChanged();
  delegate_view_->OnTabletModeChanged();
}

float HotseatWidget::CalculateOpacity() {
  const float target_opacity =
      GetShelfView()->shelf()->shelf_layout_manager()->GetOpacity();
  return (state() == HotseatState::kExtended) ? 1.0f  // fully opaque
                                              : target_opacity;
}

void HotseatWidget::SetOpaqueBackground(
    const gfx::Rect& opaque_background_bounds) {
  delegate_view_->SetOpaqueBackground(opaque_background_bounds);
}

void HotseatWidget::UpdateLayout(bool animate) {
  ui::Layer* layer = GetNativeView()->layer();
  ui::ScopedLayerAnimationSettings animation_setter(layer->GetAnimator());
  animation_setter.SetTransitionDuration(
      animate ? ShelfConfig::Get()->shelf_animation_duration()
              : base::TimeDelta::FromMilliseconds(0));
  animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
  animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  layer->SetOpacity(CalculateOpacity());
  SetBounds(
      GetShelfView()->shelf()->shelf_layout_manager()->GetHotseatBounds());
}

gfx::Size HotseatWidget::GetOpaqueBackgroundSize() const {
  DCHECK(scrollable_shelf_view_);
  return scrollable_shelf_view_->GetHotseatBackgroundBounds().size();
}

void HotseatWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

ShelfView* HotseatWidget::GetShelfView() {
  if (IsScrollableShelfEnabled()) {
    DCHECK(scrollable_shelf_view_);
    return scrollable_shelf_view_->shelf_view();
  }

  DCHECK(shelf_view_);
  return shelf_view_;
}

bool HotseatWidget::IsShowingShelfMenu() const {
  return GetShelfView()->IsShowingMenu();
}

const ShelfView* HotseatWidget::GetShelfView() const {
  return const_cast<const ShelfView*>(
      const_cast<HotseatWidget*>(this)->GetShelfView());
}

void HotseatWidget::SetState(HotseatState state) {
  if (state_ == state)
    return;

  state_ = state;

  if (!IsScrollableShelfEnabled())
    return;

  // If the hotseat is not extended we can use the normal targeting as the
  // hidden parts of the hotseat will not block non-shelf items from taking
  if (state == HotseatState::kExtended) {
    hotseat_window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
        GetNativeWindow(), std::make_unique<HotseatWindowTargeter>(this));
  } else {
    hotseat_window_targeter_.reset();
  }
}

}  // namespace ash
