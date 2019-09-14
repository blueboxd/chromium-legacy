// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_navigation_widget.h"

#include "ash/focus_cycler.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Duration of the back button's opacity animation.
constexpr auto kBackButtonOpacityAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

// Returns the bounds for the first button shown in this view (the back
// button in tablet mode, the home button otherwise).
gfx::Rect GetFirstButtonBounds() {
  return gfx::Rect(0, 0, ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

// Returns the bounds for the second button shown in this view (which is
// always the home button and only in tablet mode, which implies a horizontal
// shelf).
gfx::Rect GetSecondButtonBounds() {
  return gfx::Rect(
      ShelfConfig::Get()->control_size() + ShelfConfig::Get()->button_spacing(),
      0, ShelfConfig::Get()->control_size(),
      ShelfConfig::Get()->control_size());
}

}  // namespace

class ShelfNavigationWidget::Delegate : public views::AccessiblePaneView,
                                        public views::WidgetDelegate {
 public:
  Delegate(Shelf* shelf, ShelfView* shelf_view);
  ~Delegate() override;

  // views::View:
  FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::AccessiblePaneView:
  View* GetDefaultFocusableChild() override;

  // views::WidgetDelegate:
  bool CanActivate() const override;
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }

  BackButton* back_button() const { return back_button_; }
  HomeButton* home_button() const { return home_button_; }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

 private:
  BackButton* back_button_ = nullptr;
  HomeButton* home_button_ = nullptr;
  // When true, the default focus of the navigation widget is the last
  // focusable child.
  bool default_last_focusable_child_ = false;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

ShelfNavigationWidget::Delegate::Delegate(Shelf* shelf, ShelfView* shelf_view) {
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
}

ShelfNavigationWidget::Delegate::~Delegate() = default;

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
}

views::View* ShelfNavigationWidget::Delegate::GetDefaultFocusableChild() {
  return default_last_focusable_child_ ? GetLastFocusableChild()
                                       : GetFirstFocusableChild();
}

ShelfNavigationWidget::ShelfNavigationWidget(Shelf* shelf,
                                             ShelfView* shelf_view)
    : shelf_(shelf),
      delegate_(new ShelfNavigationWidget::Delegate(shelf, shelf_view)),
      bounds_animator_(std::make_unique<views::BoundsAnimator>(delegate_)) {
  DCHECK(shelf_);
  bounds_animator_->SetAnimationDuration(kBackButtonOpacityAnimationDuration);
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
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  Init(std::move(params));
  set_focus_on_creation(false);
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);
  SetContentsView(delegate_);
  GetBackButton()->SetBoundsRect(GetFirstButtonBounds());
  SetSize(GetIdealSize());
  UpdateLayout();
}

gfx::Size ShelfNavigationWidget::GetIdealSize() const {
  const int control_size = ShelfConfig::Get()->control_size();
  if (!shelf_->IsHorizontalAlignment())
    return gfx::Size(control_size, control_size);
  return gfx::Size(
      IsTabletMode() ? (2 * control_size + ShelfConfig::Get()->button_spacing())
                     : control_size,
      control_size);
}

bool ShelfNavigationWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active)
    delegate_->SetPaneFocusAndFocusDefault();
  return true;
}

BackButton* ShelfNavigationWidget::GetBackButton() const {
  return delegate_->back_button();
}

HomeButton* ShelfNavigationWidget::GetHomeButton() const {
  return delegate_->home_button();
}

void ShelfNavigationWidget::FocusFirstOrLastFocusableChild(bool last) {
  views::View* to_focus = GetHomeButton();
  if (IsTabletMode() && !last)
    to_focus = GetBackButton();
  GetFocusManager()->SetFocusedView(to_focus);
}

void ShelfNavigationWidget::SetDefaultLastFocusableChild(
    bool default_last_focusable_child) {
  delegate_->set_default_last_focusable_child(default_last_focusable_child);
}

void ShelfNavigationWidget::OnTabletModeStarted() {
  UpdateLayout();
}

void ShelfNavigationWidget::OnTabletModeEnded() {
  UpdateLayout();
}

void ShelfNavigationWidget::OnShelfAlignmentChanged(aura::Window* root_window) {
  UpdateLayout();
}

void ShelfNavigationWidget::OnImplicitAnimationsCompleted() {
  // Hide the back button once it has become fully transparent.
  if (!IsTabletMode())
    GetBackButton()->SetVisible(false);
}

void ShelfNavigationWidget::OnShelfConfigUpdated() {
  UpdateLayout();
}

void ShelfNavigationWidget::UpdateLayout() {
  const bool tablet_mode = IsTabletMode();
  // Show the back button right away so that the animation is visible.
  if (tablet_mode)
    GetBackButton()->SetVisible(true);
  GetBackButton()->SetFocusBehavior(tablet_mode
                                        ? views::View::FocusBehavior::ALWAYS
                                        : views::View::FocusBehavior::NEVER);
  ui::ScopedLayerAnimationSettings settings(
      GetBackButton()->layer()->GetAnimator());
  settings.SetTransitionDuration(kBackButtonOpacityAnimationDuration);
  settings.AddObserver(this);
  GetBackButton()->layer()->SetOpacity(tablet_mode ? 1 : 0);

  bounds_animator_->AnimateViewTo(
      GetHomeButton(),
      tablet_mode ? GetSecondButtonBounds() : GetFirstButtonBounds());
  GetBackButton()->SetBoundsRect(GetFirstButtonBounds());

  delegate_->SetBackground(views::CreateRoundedRectBackground(
      ShelfConfig::Get()->shelf_control_permanent_highlight_background(),
      ShelfConfig::Get()->control_border_radius()));
}

}  // namespace ash
