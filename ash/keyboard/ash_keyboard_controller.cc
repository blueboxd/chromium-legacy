// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ash_keyboard_controller.h"

#include <utility>

#include "ash/keyboard/ui/keyboard_controller.h"
#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/command_line.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

using keyboard::KeyboardConfig;
using keyboard::KeyboardEnableFlag;

namespace ash {

AshKeyboardController::AshKeyboardController(
    SessionControllerImpl* session_controller)
    : session_controller_(session_controller),
      keyboard_controller_(std::make_unique<keyboard::KeyboardController>()) {
  if (session_controller_)  // May be null in tests.
    session_controller_->AddObserver(this);
  keyboard_controller_->AddObserver(this);
}

AshKeyboardController::~AshKeyboardController() {
  keyboard_controller_->RemoveObserver(this);
  if (session_controller_)  // May be null in tests.
    session_controller_->RemoveObserver(this);
}

void AshKeyboardController::CreateVirtualKeyboard(
    std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory) {
  DCHECK(keyboard_ui_factory);
  virtual_keyboard_controller_ = std::make_unique<VirtualKeyboardController>();
  keyboard_controller_->Initialize(std::move(keyboard_ui_factory),
                                   virtual_keyboard_controller_.get());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kEnableVirtualKeyboard)) {
    keyboard_controller_->SetEnableFlag(
        KeyboardEnableFlag::kCommandLineEnabled);
  }
}

void AshKeyboardController::DestroyVirtualKeyboard() {
  virtual_keyboard_controller_.reset();
  keyboard_controller_->Shutdown();
}

void AshKeyboardController::SendOnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardVisibleBoundsChanged: " << screen_bounds.ToString();
  for (auto& observer : observers_)
    observer.OnKeyboardVisibleBoundsChanged(screen_bounds);
}

void AshKeyboardController::SendOnLoadKeyboardContentsRequested() {
  for (auto& observer : observers_)
    observer.OnLoadKeyboardContentsRequested();
}

void AshKeyboardController::SendOnKeyboardUIDestroyed() {
  for (auto& observer : observers_)
    observer.OnKeyboardUIDestroyed();
}

// ash::KeyboardController

void AshKeyboardController::KeyboardContentsLoaded(
    const gfx::Size& size) {
  keyboard_controller()->KeyboardContentsLoaded(size);
}

keyboard::KeyboardConfig AshKeyboardController::GetKeyboardConfig() {
  return keyboard_controller_->keyboard_config();
}

void AshKeyboardController::SetKeyboardConfig(
    const KeyboardConfig& keyboard_config) {
  keyboard_controller_->UpdateKeyboardConfig(keyboard_config);
}

bool AshKeyboardController::IsKeyboardEnabled() {
  return keyboard_controller_->IsEnabled();
}

void AshKeyboardController::SetEnableFlag(KeyboardEnableFlag flag) {
  keyboard_controller_->SetEnableFlag(flag);
}

void AshKeyboardController::ClearEnableFlag(KeyboardEnableFlag flag) {
  keyboard_controller_->ClearEnableFlag(flag);
}

const std::set<keyboard::KeyboardEnableFlag>&
AshKeyboardController::GetEnableFlags() {
  return keyboard_controller_->keyboard_enable_flags();
}

void AshKeyboardController::ReloadKeyboardIfNeeded() {
  keyboard_controller_->Reload();
}

void AshKeyboardController::RebuildKeyboardIfEnabled() {
  // Test IsKeyboardEnableRequested in case of an unlikely edge case where this
  // is called while after the enable state changed to disabled (in which case
  // we do not want to override the requested state).
  keyboard_controller_->RebuildKeyboardIfEnabled();
}

bool AshKeyboardController::IsKeyboardVisible() {
  return keyboard_controller_->IsKeyboardVisible();
}

void AshKeyboardController::ShowKeyboard() {
  if (keyboard_controller_->IsEnabled())
    keyboard_controller_->ShowKeyboard(false /* lock */);
}

void AshKeyboardController::HideKeyboard(HideReason reason) {
  if (!keyboard_controller_->IsEnabled())
    return;
  switch (reason) {
    case HideReason::kUser:
      keyboard_controller_->HideKeyboardByUser();
      break;
    case HideReason::kSystem:
      keyboard_controller_->HideKeyboardExplicitlyBySystem();
      break;
  }
}

void AshKeyboardController::SetContainerType(
    keyboard::ContainerType container_type,
    const base::Optional<gfx::Rect>& target_bounds,
    SetContainerTypeCallback callback) {
  keyboard_controller_->SetContainerType(container_type, target_bounds,
                                         std::move(callback));
}

void AshKeyboardController::SetKeyboardLocked(bool locked) {
  keyboard_controller_->set_keyboard_locked(locked);
}

void AshKeyboardController::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  // TODO(https://crbug.com/826617): Support occluded bounds with multiple
  // rectangles.
  keyboard_controller_->SetOccludedBounds(bounds.empty() ? gfx::Rect()
                                                         : bounds[0]);
}

void AshKeyboardController::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_controller_->SetHitTestBounds(bounds);
}

void AshKeyboardController::SetDraggableArea(const gfx::Rect& bounds) {
  keyboard_controller_->SetDraggableArea(bounds);
}

void AshKeyboardController::AddObserver(KeyboardControllerObserver* observer) {
  observers_.AddObserver(observer);
}

// SessionObserver
void AshKeyboardController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (!keyboard_controller_->IsEnabled())
    return;

  switch (state) {
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
      // Reload the keyboard on user profile change to refresh keyboard
      // extensions with the new profile and ensure the extensions call the
      // proper IME. |LOGGED_IN_NOT_ACTIVE| is needed so that the virtual
      // keyboard works on supervised user creation, http://crbug.com/712873.
      // |ACTIVE| is also needed for guest user workflow.
      RebuildKeyboardIfEnabled();
      break;
    default:
      break;
  }
}

// private methods

void AshKeyboardController::OnRootWindowClosing(aura::Window* root_window) {
  if (keyboard_controller_->GetRootWindow() == root_window) {
    aura::Window* new_parent =
        virtual_keyboard_controller_->GetContainerForDefaultDisplay();
    DCHECK_NE(root_window, new_parent);
    keyboard_controller_->MoveToParentContainer(new_parent);
  }
}

void AshKeyboardController::OnKeyboardConfigChanged(
    const keyboard::KeyboardConfig& config) {
  for (auto& observer : observers_)
    observer.OnKeyboardConfigChanged(config);
}

void AshKeyboardController::OnKeyboardVisibilityChanged(bool is_visible) {
  for (auto& observer : observers_)
    observer.OnKeyboardVisibilityChanged(is_visible);
}

void AshKeyboardController::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  SendOnKeyboardVisibleBoundsChanged(screen_bounds);
}

void AshKeyboardController::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardOccludedBoundsChanged: " << screen_bounds.ToString();
  for (auto& observer : observers_)
    observer.OnKeyboardOccludedBoundsChanged(screen_bounds);
}

void AshKeyboardController::OnKeyboardEnableFlagsChanged(
    const std::set<keyboard::KeyboardEnableFlag>& flags) {
  for (auto& observer : observers_)
    observer.OnKeyboardEnableFlagsChanged(flags);
}

void AshKeyboardController::OnKeyboardEnabledChanged(bool is_enabled) {
  for (auto& observer : observers_)
    observer.OnKeyboardEnabledChanged(is_enabled);
}

}  // namespace ash
