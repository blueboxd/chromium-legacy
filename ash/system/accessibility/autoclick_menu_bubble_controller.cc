// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/accessibility/autoclick_scroll_bubble_controller.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_utils.h"
#include "ui/views/bubble/bubble_border.h"

namespace ash {

namespace {
// Autoclick menu constants.
const int kAutoclickMenuWidth = 321;
const int kAutoclickMenuWidthWithScroll = 371;
const int kAutoclickMenuHeight = 64;

mojom::AutoclickMenuPosition DefaultSystemPosition() {
  return base::i18n::IsRTL() ? mojom::AutoclickMenuPosition::kBottomLeft
                             : mojom::AutoclickMenuPosition::kBottomRight;
}

views::BubbleBorder::Arrow GetScrollAnchorAlignmentForPosition(
    mojom::AutoclickMenuPosition position) {
  // If this is the default system position, pick the position based on the
  // language direction.
  if (position == mojom::AutoclickMenuPosition::kSystemDefault) {
    position = DefaultSystemPosition();
  }
  // Mirror arrow in RTL languages so that it always stays near the screen
  // edge.
  switch (position) {
    case mojom::AutoclickMenuPosition::kBottomLeft:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::TOP_RIGHT
                                 : views::BubbleBorder::Arrow::TOP_LEFT;
    case mojom::AutoclickMenuPosition::kTopLeft:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::BOTTOM_RIGHT
                                 : views::BubbleBorder::Arrow::BOTTOM_LEFT;
    case mojom::AutoclickMenuPosition::kBottomRight:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::TOP_LEFT
                                 : views::BubbleBorder::Arrow::TOP_RIGHT;
    case mojom::AutoclickMenuPosition::kTopRight:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::BOTTOM_LEFT
                                 : views::BubbleBorder::Arrow::BOTTOM_RIGHT;
    case mojom::AutoclickMenuPosition::kSystemDefault:
      // It's not possible for position to be kSystemDefault here because we've
      // set it via DefaultSystemPosition() above if it was kSystemDefault.
      NOTREACHED();
      return views::BubbleBorder::Arrow::TOP_LEFT;
  }
}

}  // namespace

AutoclickMenuBubbleController::AutoclickMenuBubbleController() {
  Shell::Get()->locale_update_controller()->AddObserver(this);
}

AutoclickMenuBubbleController::~AutoclickMenuBubbleController() {
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
  Shell::Get()->locale_update_controller()->RemoveObserver(this);
  scroll_bubble_controller_.reset();
}

void AutoclickMenuBubbleController::SetEventType(
    mojom::AutoclickEventType type) {
  if (menu_view_) {
    menu_view_->UpdateEventType(type);
  }

  if (type == mojom::AutoclickEventType::kScroll) {
    // If the type is scroll, show the scroll bubble using the
    // scroll_bubble_controller_.
    if (!scroll_bubble_controller_) {
      scroll_bubble_controller_ =
          std::make_unique<AutoclickScrollBubbleController>();
    }
    gfx::Rect anchor_rect = bubble_view_->GetBoundsInScreen();
    anchor_rect.Inset(-kCollisionWindowWorkAreaInsetsDp,
                      -kCollisionWindowWorkAreaInsetsDp);
    scroll_bubble_controller_->ShowBubble(
        anchor_rect, GetScrollAnchorAlignmentForPosition(position_));
  } else {
    scroll_bubble_controller_ = nullptr;
  }
}

void AutoclickMenuBubbleController::SetPosition(
    mojom::AutoclickMenuPosition new_position) {
  if (!menu_view_ || !bubble_view_ || !bubble_widget_)
    return;

  // Update the menu view's UX if the position has changed, or if it's not the
  // default position (because that can change with language direction).
  if (position_ != new_position ||
      new_position == mojom::AutoclickMenuPosition::kSystemDefault) {
    menu_view_->UpdatePosition(new_position);
  }
  position_ = new_position;

  // If this is the default system position, pick the position based on the
  // language direction.
  if (new_position == mojom::AutoclickMenuPosition::kSystemDefault) {
    new_position = DefaultSystemPosition();
  }

  // Calculates the ideal bounds.
  // TODO(katie): Support multiple displays: draw the menu on whichever display
  // the cursor is on.
  aura::Window* window = Shell::GetPrimaryRootWindow();
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
  int width = base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kEnableExperimentalAccessibilityAutoclick)
                  ? kAutoclickMenuWidthWithScroll
                  : kAutoclickMenuWidth;
  gfx::Rect new_bounds;
  switch (new_position) {
    case mojom::AutoclickMenuPosition::kBottomRight:
      new_bounds = gfx::Rect(work_area.right() - width,
                             work_area.bottom() - kAutoclickMenuHeight, width,
                             kAutoclickMenuHeight);
      break;
    case mojom::AutoclickMenuPosition::kBottomLeft:
      new_bounds =
          gfx::Rect(work_area.x(), work_area.bottom() - kAutoclickMenuHeight,
                    width, kAutoclickMenuHeight);
      break;
    case mojom::AutoclickMenuPosition::kTopLeft:
      // Setting the top to 1 instead of 0 so that the view is drawn on screen.
      new_bounds = gfx::Rect(work_area.x(), 1, width, kAutoclickMenuHeight);
      break;
    case mojom::AutoclickMenuPosition::kTopRight:
      // Setting the top to 1 instead of 0 so that the view is drawn on screen.
      new_bounds =
          gfx::Rect(work_area.right() - width, 1, width, kAutoclickMenuHeight);
      break;
    case mojom::AutoclickMenuPosition::kSystemDefault:
      return;
  }

  // Update the preferred bounds based on other system windows.
  gfx::Rect resting_bounds = CollisionDetectionUtils::GetRestingPosition(
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          bubble_widget_->GetNativeWindow()),
      new_bounds,
      CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu);

  // Un-inset the bounds to get the widget's bounds, which includes the drop
  // shadow.
  resting_bounds.Inset(-kCollisionWindowWorkAreaInsetsDp,
                       -kCollisionWindowWorkAreaInsetsDp);
  if (bubble_widget_->GetWindowBoundsInScreen() == resting_bounds)
    return;

  ui::ScopedLayerAnimationSettings settings(
      bubble_widget_->GetLayer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  bubble_widget_->SetBounds(resting_bounds);

  if (!scroll_bubble_controller_)
    return;

  // Position the scroll bubble with respect to the menu.
  scroll_bubble_controller_->UpdateAnchorRect(
      resting_bounds, GetScrollAnchorAlignmentForPosition(new_position));
}

void AutoclickMenuBubbleController::ShowBubble(
    mojom::AutoclickEventType type,
    mojom::AutoclickMenuPosition position) {
  // Ignore if bubble widget already exists.
  if (bubble_widget_)
    return;

  DCHECK(!bubble_view_);

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  // Anchor within the overlay container.
  init_params.parent_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AutoclickContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.insets = gfx::Insets(kCollisionWindowWorkAreaInsetsDp);
  int width = base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kEnableExperimentalAccessibilityAutoclick)
                  ? kAutoclickMenuWidthWithScroll
                  : kAutoclickMenuWidth;
  init_params.min_width = width;
  init_params.max_width = width;
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;
  bubble_view_ = new AutoclickMenuBubbleView(init_params);

  menu_view_ = new AutoclickMenuView(type, position);
  menu_view_->SetBackground(UnifiedSystemTrayView::CreateBackground());
  menu_view_->SetBorder(
      views::CreateEmptyBorder(kUnifiedTopShortcutSpacing, 0, 0, 0));
  bubble_view_->AddChildView(menu_view_);
  bubble_view_->set_color(SK_ColorTRANSPARENT);
  bubble_view_->layer()->SetFillsBoundsOpaquely(false);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      bubble_widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu);
  bubble_view_->InitializeAndShowBubble();

  if (app_list_features::IsBackgroundBlurEnabled()) {
    bubble_widget_->client_view()->layer()->SetBackgroundBlur(
        kUnifiedMenuBackgroundBlur);
  }

  SetPosition(position);
}

void AutoclickMenuBubbleController::CloseBubble() {
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
}

void AutoclickMenuBubbleController::SetBubbleVisibility(bool is_visible) {
  if (!bubble_widget_)
    return;

  if (is_visible)
    bubble_widget_->Show();
  else
    bubble_widget_->Hide();

  if (!scroll_bubble_controller_)
    return;
  scroll_bubble_controller_->SetBubbleVisibility(is_visible);
}

void AutoclickMenuBubbleController::ClickOnBubble(gfx::Point location_in_dips,
                                                  int mouse_event_flags) {
  if (!bubble_widget_)
    return;

  // Change the event location bounds to be relative to the menu bubble.
  location_in_dips -= bubble_view_->GetBoundsInScreen().OffsetFromOrigin();

  // Generate synthesized mouse events for the click.
  const ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, location_in_dips,
                                   location_in_dips, ui::EventTimeForNow(),
                                   mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON);
  const ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, location_in_dips, location_in_dips,
      ui::EventTimeForNow(), mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);

  // Send the press/release events to the widget's root view for processing.
  bubble_widget_->GetRootView()->OnMousePressed(press_event);
  bubble_widget_->GetRootView()->OnMouseReleased(release_event);
}

void AutoclickMenuBubbleController::ClickOnScrollBubble(
    gfx::Point location_in_dips,
    int mouse_event_flags) {
  if (!scroll_bubble_controller_)
    return;

  scroll_bubble_controller_->ClickOnBubble(location_in_dips, mouse_event_flags);
}

bool AutoclickMenuBubbleController::ContainsPointInScreen(
    const gfx::Point& point) {
  return bubble_view_ && bubble_view_->GetBoundsInScreen().Contains(point);
}

bool AutoclickMenuBubbleController::ScrollBubbleContainsPointInScreen(
    const gfx::Point& point) {
  return scroll_bubble_controller_ &&
         scroll_bubble_controller_->ContainsPointInScreen(point);
}

void AutoclickMenuBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  menu_view_ = nullptr;
}

void AutoclickMenuBubbleController::OnLocaleChanged() {
  // Layout update is needed when language changes between LTR and RTL, if the
  // position is the system default.
  if (position_ == mojom::AutoclickMenuPosition::kSystemDefault)
    SetPosition(position_);
}

}  // namespace ash
