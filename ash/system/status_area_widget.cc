// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/select_to_speak_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/status_area_overflow_button_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/i18n/time_formatting.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/display/display.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace ash {

StatusAreaWidget::StatusAreaWidget(aura::Window* status_container, Shelf* shelf)
    : status_area_widget_delegate_(new StatusAreaWidgetDelegate(shelf)),
      shelf_(shelf) {
  DCHECK(status_container);
  DCHECK(shelf);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.name = "StatusAreaWidget";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = status_container;
  Init(std::move(params));
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
}

void StatusAreaWidget::Initialize() {
  DCHECK(!initialized_);

  // Create the child views, left to right.

  overflow_button_tray_ =
      std::make_unique<StatusAreaOverflowButtonTray>(shelf_);
  AddTrayButton(overflow_button_tray_.get());

  logout_button_tray_ = std::make_unique<LogoutButtonTray>(shelf_);
  AddTrayButton(logout_button_tray_.get());

  dictation_button_tray_ = std::make_unique<DictationButtonTray>(shelf_);
  AddTrayButton(dictation_button_tray_.get());

  select_to_speak_tray_ = std::make_unique<SelectToSpeakTray>(shelf_);
  AddTrayButton(select_to_speak_tray_.get());

  ime_menu_tray_ = std::make_unique<ImeMenuTray>(shelf_);
  AddTrayButton(ime_menu_tray_.get());

  virtual_keyboard_tray_ = std::make_unique<VirtualKeyboardTray>(shelf_);
  AddTrayButton(virtual_keyboard_tray_.get());

  palette_tray_ = std::make_unique<PaletteTray>(shelf_);
  AddTrayButton(palette_tray_.get());

  unified_system_tray_ = std::make_unique<UnifiedSystemTray>(shelf_);
  AddTrayButton(unified_system_tray_.get());

  overview_button_tray_ = std::make_unique<OverviewButtonTray>(shelf_);
  AddTrayButton(overview_button_tray_.get());

  // The layout depends on the number of children, so build it once after
  // adding all of them.
  status_area_widget_delegate_->UpdateLayout();

  // Initialize after all trays have been created.
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->Initialize();

  UpdateAfterShelfAlignmentChange();
  UpdateAfterLoginStatusChange(
      Shell::Get()->session_controller()->login_status());

  ShelfConfig::Get()->AddObserver(this);

  // NOTE: Container may be hidden depending on login/display state.
  Show();

  initialized_ = true;
}

StatusAreaWidget::~StatusAreaWidget() {
  ShelfConfig::Get()->RemoveObserver(this);
}

void StatusAreaWidget::UpdateAfterShelfAlignmentChange() {
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateAfterShelfChange();
  status_area_widget_delegate_->UpdateLayout();
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;

  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateAfterLoginStatusChange(login_status);
}

void StatusAreaWidget::SetSystemTrayVisibility(bool visible) {
  TrayBackgroundView* tray = unified_system_tray_.get();
  tray->SetVisiblePreferred(visible);
  // Opacity is set to prevent flakiness in kiosk browser tests. See
  // https://crbug.com/624584.
  SetOpacity(visible ? 1.f : 0.f);
  if (visible) {
    Show();
  } else {
    tray->CloseBubble();
    Hide();
  }
}

void StatusAreaWidget::UpdateCollapseState() {
  // The status area is only collapsible in tablet mode. Otherwise, we just show
  // all trays.
  if (!Shell::Get()->tablet_mode_controller())
    return;

  // An update may occur during initialization of the shelf, so just skip it.
  if (!initialized_)
    return;

  bool is_collapsible =
      chromeos::switches::ShouldShowShelfHotseat() &&
      Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      ShelfConfig::Get()->is_in_app();

  bool force_collapsible = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshForceStatusAreaCollapsible);

  is_collapsible |= force_collapsible;
  if (is_collapsible) {
    // Update the collapse state based on the previous overflow button state.
    collapse_state_ = overflow_button_tray_->state() ==
                              StatusAreaOverflowButtonTray::CLICK_TO_EXPAND
                          ? CollapseState::COLLAPSED
                          : CollapseState::EXPANDED;
  } else {
    collapse_state_ = CollapseState::NOT_COLLAPSIBLE;
  }

  if (collapse_state_ == CollapseState::COLLAPSED) {
    CalculateButtonVisibilityForCollapsedState();
  } else {
    // All tray buttons are visible when the status area is not collapsible.
    // This is the most common state. They are also all visible when the status
    // area is expanded by the user.
    overflow_button_tray_->SetVisiblePreferred(collapse_state_ ==
                                               CollapseState::EXPANDED);
    for (TrayBackgroundView* tray_button : tray_buttons_) {
      tray_button->set_show_when_collapsed(true);
      tray_button->UpdateAfterStatusAreaCollapseChange();
    }
  }
}

void StatusAreaWidget::CalculateButtonVisibilityForCollapsedState() {
  DCHECK(collapse_state_ == CollapseState::COLLAPSED);

  bool force_collapsible = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshForceStatusAreaCollapsible);

  // We update visibility of each tray button based on the available width.
  int shelf_width =
      shelf_->shelf_widget()->GetClientAreaBoundsInScreen().width();
  int available_width =
      force_collapsible ? kStatusAreaForceCollapseAvailableWidth
                        : shelf_width / 2 - kStatusAreaLeftPaddingForOverflow;

  // First, reset all tray button to be hidden.
  overflow_button_tray_->ResetStateToCollapsed();
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->set_show_when_collapsed(false);

  // Iterate backwards making tray buttons visible until |available_width| is
  // exceeded.
  TrayBackgroundView* previous_tray = nullptr;
  bool show_overflow_button = false;
  int used_width = 0;
  for (TrayBackgroundView* tray : base::Reversed(tray_buttons_)) {
    // If we reach the final overflow tray button, then all the tray buttons fit
    // and there is no need for a collapse state.
    if (tray == overflow_button_tray_.get()) {
      collapse_state_ = CollapseState::NOT_COLLAPSIBLE;
      break;
    }

    // Skip non-enabled tray buttons.
    if (!tray->visible_preferred())
      continue;

    // Show overflow button once available width is exceeded.
    int tray_width = tray->tray_container()->GetPreferredSize().width();
    if (used_width + tray_width > available_width) {
      show_overflow_button = true;

      // Maybe remove the last tray button to make rooom for the overflow tray.
      int overflow_button_width =
          overflow_button_tray_->GetPreferredSize().width();
      if (previous_tray && used_width + overflow_button_width > available_width)
        previous_tray->set_show_when_collapsed(false);
      break;
    }

    tray->set_show_when_collapsed(true);
    previous_tray = tray;
    used_width += tray_width;
  }

  overflow_button_tray_->SetVisiblePreferred(show_overflow_button);
  overflow_button_tray_->UpdateAfterStatusAreaCollapseChange();
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateAfterStatusAreaCollapseChange();
}

TrayBackgroundView* StatusAreaWidget::GetSystemTrayAnchor() const {
  // Use the target visibility of the layer instead of the visibility of the
  // view because the view is still visible when fading away, but we do not want
  // to anchor to this element in that case.
  if (overview_button_tray_->layer()->GetTargetVisibility())
    return overview_button_tray_.get();

  return unified_system_tray_.get();
}

bool StatusAreaWidget::ShouldShowShelf() const {
  // If it has main bubble, return true.
  if (unified_system_tray_->IsBubbleShown())
    return true;

  // If it has a slider bubble, return false.
  if (unified_system_tray_->IsSliderBubbleShown())
    return false;

  // All other tray bubbles will force the shelf to be visible.
  return TrayBubbleView::IsATrayBubbleOpen();
}

bool StatusAreaWidget::IsMessageBubbleShown() const {
  return unified_system_tray_->IsBubbleShown();
}

void StatusAreaWidget::SchedulePaint() {
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->SchedulePaint();
}

const ui::NativeTheme* StatusAreaWidget::GetNativeTheme() const {
  return ui::NativeThemeDarkAura::instance();
}

bool StatusAreaWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active)
    status_area_widget_delegate_->SetPaneFocusAndFocusDefault();
  return true;
}

void StatusAreaWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event);
    return;
  }

  // Clicking anywhere except the virtual keyboard tray icon should hide the
  // virtual keyboard.
  gfx::Point location = event->location();
  views::View::ConvertPointFromWidget(virtual_keyboard_tray_.get(), &location);
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      !virtual_keyboard_tray_->HitTestPoint(location)) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  }
  views::Widget::OnMouseEvent(event);
}

void StatusAreaWidget::OnGestureEvent(ui::GestureEvent* event) {
  // Tapping anywhere except the virtual keyboard tray icon should hide the
  // virtual keyboard.
  gfx::Point location = event->location();
  views::View::ConvertPointFromWidget(virtual_keyboard_tray_.get(), &location);
  if (event->type() == ui::ET_GESTURE_TAP_DOWN &&
      !virtual_keyboard_tray_->HitTestPoint(location)) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  }
  views::Widget::OnGestureEvent(event);
}

void StatusAreaWidget::OnShelfConfigUpdated() {
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateAfterShelfChange();
  UpdateCollapseState();
}

void StatusAreaWidget::AddTrayButton(TrayBackgroundView* tray_button) {
  status_area_widget_delegate_->AddChildView(tray_button);
  tray_buttons_.push_back(tray_button);
}

}  // namespace ash
