// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_navigation_widget.h"

#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The duration of the back button opacity animation.
constexpr base::TimeDelta kButtonOpacityAnimationDuration =
    base::TimeDelta::FromMilliseconds(50);

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

// Returns the bounds for the first button shown in this view (the back
// button in tablet mode, the home button otherwise).
gfx::Rect GetFirstButtonBounds() {
  // ShelfNavigationWidget is larger than the buttons in order to enable child
  // views to capture events nearby.
  const int home_button_edge_spacing =
      ShelfConfig::Get()->home_button_edge_spacing();
  return gfx::Rect(home_button_edge_spacing, home_button_edge_spacing,
                   ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

// Returns the bounds for the second button shown in this view (which is
// always the home button and only in tablet mode, which implies a horizontal
// shelf).
gfx::Rect GetSecondButtonBounds() {
  const int home_button_edge_spacing =
      ShelfConfig::Get()->home_button_edge_spacing();
  return gfx::Rect(home_button_edge_spacing +
                       ShelfConfig::Get()->control_size() +
                       ShelfConfig::Get()->button_spacing(),
                   home_button_edge_spacing, ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

bool IsBackButtonShown() {
  if (!ShelfConfig::Get()->shelf_controls_shown())
    return false;
  return chromeos::switches::ShouldShowShelfHotseat()
             ? IsTabletMode() && ShelfConfig::Get()->is_in_app()
             : IsTabletMode();
}

bool IsHomeButtonShown() {
  return ShelfConfig::Get()->shelf_controls_shown();
}

// An implicit animation observer that hides a view once the view's opacity
// animation finishes.
// It deletes itself when the animation is done.
class AnimationObserverToHideView : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverToHideView(views::View* view) : view_(view) {}
  ~AnimationObserverToHideView() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (view_->layer()->GetTargetOpacity() == 0.0f)
      view_->SetVisible(false);
    delete this;
  }

 private:
  views::View* const view_;
};

}  // namespace

class ShelfNavigationWidget::Delegate : public views::AccessiblePaneView,
                                        public views::WidgetDelegate {
 public:
  Delegate(Shelf* shelf, ShelfView* shelf_view);
  ~Delegate() override;

  // Initializes the view.
  void Init(ui::Layer* parent_layer);

  void UpdateOpaqueBackground();

  // views::View:
  FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;

  // views::AccessiblePaneView:
  View* GetDefaultFocusableChild() override;

  // views::WidgetDelegate:
  bool CanActivate() const override;

  BackButton* back_button() const { return back_button_; }
  HomeButton* home_button() const { return home_button_; }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

 private:
  void SetParentLayer(ui::Layer* layer);

  BackButton* back_button_ = nullptr;
  HomeButton* home_button_ = nullptr;
  // When true, the default focus of the navigation widget is the last
  // focusable child.
  bool default_last_focusable_child_ = false;

  // A background layer that may be visible depending on shelf state.
  ui::Layer opaque_background_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

ShelfNavigationWidget::Delegate::Delegate(Shelf* shelf, ShelfView* shelf_view)
    : opaque_background_(ui::LAYER_SOLID_COLOR) {
  set_allow_deactivate_on_esc(true);

  const int control_size = ShelfConfig::Get()->control_size();
  std::unique_ptr<BackButton> back_button_ptr =
      std::make_unique<BackButton>(shelf);
  back_button_ = AddChildView(std::move(back_button_ptr));
  back_button_->SetSize(gfx::Size(control_size, control_size));

  std::unique_ptr<HomeButton> home_button_ptr =
      std::make_unique<HomeButton>(shelf);
  home_button_ = AddChildView(std::move(home_button_ptr));
  home_button_->set_context_menu_controller(shelf_view);
  home_button_->SetSize(gfx::Size(control_size, control_size));

  GetViewAccessibility().OverrideNextFocus(
      shelf->shelf_widget()->hotseat_widget());
  GetViewAccessibility().OverridePreviousFocus(shelf->GetStatusAreaWidget());
  opaque_background_.SetName("shelfNavigation/Background");
}

ShelfNavigationWidget::Delegate::~Delegate() = default;

void ShelfNavigationWidget::Delegate::Init(ui::Layer* parent_layer) {
  SetParentLayer(parent_layer);
  UpdateOpaqueBackground();
}

void ShelfNavigationWidget::Delegate::UpdateOpaqueBackground() {
  opaque_background_.SetColor(ShelfConfig::Get()->GetShelfControlButtonColor());

  if (chromeos::switches::ShouldShowShelfHotseat() && IsTabletMode() &&
      ShelfConfig::Get()->is_in_app()) {
    opaque_background_.SetVisible(false);
    return;
  }
  opaque_background_.SetVisible(true);

  int radius = ShelfConfig::Get()->control_border_radius();
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  if (opaque_background_.rounded_corner_radii() != rounded_corners)
    opaque_background_.SetRoundedCornerRadius(rounded_corners);

  // The opaque background does not show up when there are two buttons.
  gfx::Rect opaque_background_bounds = GetFirstButtonBounds();
  if (base::i18n::IsRTL() && GetWidget() &&
      Shelf::ForWindow(GetWidget()->GetNativeWindow())
          ->IsHorizontalAlignment()) {
    opaque_background_bounds.set_x(
        2 * ShelfConfig::Get()->home_button_edge_spacing());
  }
  opaque_background_.SetBounds(opaque_background_bounds);
  opaque_background_.SetBackgroundBlur(
      ShelfConfig::Get()->GetShelfControlButtonBlurRadius());
}

bool ShelfNavigationWidget::Delegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return Shell::Get()->focus_cycler()->widget_activating() == GetWidget();
}

views::FocusTraversable*
ShelfNavigationWidget::Delegate::GetPaneFocusTraversable() {
  return this;
}

void ShelfNavigationWidget::Delegate::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));

  ShelfWidget* shelf_widget =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget();
  GetViewAccessibility().OverrideNextFocus(shelf_widget->hotseat_widget());
  GetViewAccessibility().OverridePreviousFocus(
      shelf_widget->status_area_widget());
}

void ShelfNavigationWidget::Delegate::ReorderChildLayers(
    ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
}

void ShelfNavigationWidget::Delegate::OnBoundsChanged(
    const gfx::Rect& old_bounds) {
  UpdateOpaqueBackground();
}

views::View* ShelfNavigationWidget::Delegate::GetDefaultFocusableChild() {
  return default_last_focusable_child_ ? GetLastFocusableChild()
                                       : GetFirstFocusableChild();
}

void ShelfNavigationWidget::Delegate::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  ReorderLayers();
}

ShelfNavigationWidget::TestApi::TestApi(ShelfNavigationWidget* widget)
    : navigation_widget_(widget) {}

ShelfNavigationWidget::TestApi::~TestApi() = default;

bool ShelfNavigationWidget::TestApi::IsHomeButtonVisible() const {
  const HomeButton* button = navigation_widget_->delegate_->home_button();
  const float opacity = button->layer()->GetTargetOpacity();
  DCHECK(opacity == 0.0f || opacity == 1.0f)
      << "Unexpected opacity " << opacity;
  return opacity > 0.0f && button->GetVisible();
}

bool ShelfNavigationWidget::TestApi::IsBackButtonVisible() const {
  const BackButton* button = navigation_widget_->delegate_->back_button();
  const float opacity = button->layer()->GetTargetOpacity();
  DCHECK(opacity == 0.0f || opacity == 1.0f)
      << "Unexpected opacity " << opacity;
  return opacity > 0.0f && button->GetVisible();
}

views::BoundsAnimator* ShelfNavigationWidget::TestApi::GetBoundsAnimator() {
  return navigation_widget_->bounds_animator_.get();
}

ShelfNavigationWidget::ShelfNavigationWidget(Shelf* shelf,
                                             ShelfView* shelf_view)
    : shelf_(shelf),
      delegate_(new ShelfNavigationWidget::Delegate(shelf, shelf_view)),
      bounds_animator_(std::make_unique<views::BoundsAnimator>(delegate_)) {
  DCHECK(shelf_);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  ShelfConfig::Get()->AddObserver(this);
}

ShelfNavigationWidget::~ShelfNavigationWidget() {
  // Shell destroys the TabletModeController before destroying all root windows.
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  ShelfConfig::Get()->RemoveObserver(this);
}

void ShelfNavigationWidget::Initialize(aura::Window* container) {
  DCHECK(container);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "ShelfNavigationWidget";
  params.delegate = delegate_;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  Init(std::move(params));
  delegate_->Init(GetLayer());
  set_focus_on_creation(false);
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);
  SetContentsView(delegate_);
  SetSize(GetIdealSize());
  UpdateLayout(/*animate=*/false);
}

gfx::Size ShelfNavigationWidget::GetIdealSize() const {
  const int button_count =
      (IsBackButtonShown() ? 1 : 0) + (IsHomeButtonShown() ? 1 : 0);

  if (button_count == 0)
    return gfx::Size();

  const int control_size = ShelfConfig::Get()->control_size();
  const int home_button_edge_spacing =
      ShelfConfig::Get()->home_button_edge_spacing();

  if (!shelf_->IsHorizontalAlignment()) {
    return gfx::Size(home_button_edge_spacing + control_size,
                     home_button_edge_spacing + control_size);
  }

  gfx::Size ideal_size(
      button_count * control_size +
          (button_count - 1) * ShelfConfig::Get()->button_spacing(),
      control_size);
  if (IsHomeButtonShown()) {
    ideal_size.Enlarge(2 * home_button_edge_spacing,
                       2 * home_button_edge_spacing);
  }

  return ideal_size;
}

void ShelfNavigationWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event);
    return;
  }

  views::Widget::OnMouseEvent(event);
}

bool ShelfNavigationWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active)
    delegate_->SetPaneFocusAndFocusDefault();
  return true;
}

void ShelfNavigationWidget::OnGestureEvent(ui::GestureEvent* event) {
  // Shelf::ProcessGestureEvent expects an event whose location is in screen
  // coordinates - create a copy of the event with the location in screen
  // coordinate system.
  ui::GestureEvent copy_event(*event);
  gfx::Point location_in_screen(copy_event.location());
  wm::ConvertPointToScreen(GetNativeWindow(), &location_in_screen);
  copy_event.set_location(location_in_screen);

  if (shelf_->ProcessGestureEvent(copy_event)) {
    event->StopPropagation();
    return;
  }
  views::Widget::OnGestureEvent(event);
}

BackButton* ShelfNavigationWidget::GetBackButton() const {
  return IsBackButtonShown() ? delegate_->back_button() : nullptr;
}

HomeButton* ShelfNavigationWidget::GetHomeButton() const {
  return IsHomeButtonShown() ? delegate_->home_button() : nullptr;
}

void ShelfNavigationWidget::SetDefaultLastFocusableChild(
    bool default_last_focusable_child) {
  delegate_->set_default_last_focusable_child(default_last_focusable_child);
}

void ShelfNavigationWidget::OnTabletModeStarted() {
  UpdateLayout(/*animate=*/true);
}

void ShelfNavigationWidget::OnTabletModeEnded() {
  UpdateLayout(/*animate=*/true);
}

void ShelfNavigationWidget::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  UpdateLayout(/*animate=*/false);
}

void ShelfNavigationWidget::OnShelfConfigUpdated() {
  UpdateLayout(/*animate=*/true);
}

void ShelfNavigationWidget::UpdateLayout(bool animate) {
  const bool back_button_shown = IsBackButtonShown();
  const bool home_button_shown = IsHomeButtonShown();

  // If the widget is currently active, and all the buttons will be hidden,
  // focus out to the status area (the widget's focus manager does not properly
  // handle the case where the widget does not have another view to focus - it
  // would clear the focus, and hit a DCHECK trying to cycle focus within the
  // widget).
  if (IsActive() && !back_button_shown && !home_button_shown) {
    Shelf::ForWindow(GetNativeWindow())
        ->shelf_focus_cycler()
        ->FocusOut(true /*reverse*/, SourceView::kShelfNavigationView);
  }

  // Use the same duration for all parts of the upcoming animation.
  const auto animation_duration =
      animate ? ShelfConfig::Get()->shelf_animation_duration()
              : base::TimeDelta::FromMilliseconds(0);
  bounds_animator_->SetAnimationDuration(animation_duration);

  ui::ScopedLayerAnimationSettings nav_animation_setter(
      GetNativeView()->layer()->GetAnimator());
  nav_animation_setter.SetTransitionDuration(animation_duration);
  nav_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
  nav_animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  SetBounds(shelf_->shelf_layout_manager()->GetNavigationBounds());

  views::View* const back_button = delegate_->back_button();
  UpdateButtonVisibility(back_button, back_button_shown, animate);

  views::View* const home_button = delegate_->home_button();
  UpdateButtonVisibility(home_button, home_button_shown, animate);

  gfx::Rect home_button_bounds =
      back_button_shown ? GetSecondButtonBounds() : GetFirstButtonBounds();
  if (animate)
    bounds_animator_->AnimateViewTo(home_button, home_button_bounds);
  else
    home_button->SetBoundsRect(home_button_bounds);

  back_button->SetBoundsRect(GetFirstButtonBounds());

  delegate_->UpdateOpaqueBackground();
}

void ShelfNavigationWidget::UpdateButtonVisibility(views::View* button,
                                                   bool visible,
                                                   bool animate) {
  // Update visibility immediately only if making the button visible. When
  // hiding the button, the visibility will be updated when the animations
  // complete (by AnimationObserverToHideView).
  if (visible)
    button->SetVisible(true);
  button->SetFocusBehavior(visible ? views::View::FocusBehavior::ALWAYS
                                   : views::View::FocusBehavior::NEVER);

  ui::ScopedLayerAnimationSettings opacity_settings(
      button->layer()->GetAnimator());
  opacity_settings.SetTransitionDuration(
      animate ? kButtonOpacityAnimationDuration : base::TimeDelta());
  opacity_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (!visible)
    opacity_settings.AddObserver(new AnimationObserverToHideView(button));

  button->layer()->SetOpacity(visible ? 1.0f : 0.0f);
}

}  // namespace ash
