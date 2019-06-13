// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
#define ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_

#include <memory>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/optional.h"

namespace gfx {
class Rect;
}

namespace keyboard {
class KeyboardController;
class KeyboardUIFactory;
}

namespace ash {

class SessionControllerImpl;
class VirtualKeyboardController;

// Contains and observes a keyboard::KeyboardController instance. Ash specific
// behavior, including implementing the public interface, is implemented in this
// class. TODO(shend): Consider re-factoring keyboard::KeyboardController so
// that this can inherit from that class instead. Rename this to
// KeyboardControllerImpl.
class ASH_EXPORT AshKeyboardController : public KeyboardController,
                                         public KeyboardControllerObserver,
                                         public SessionObserver {
 public:
  // |session_controller| is expected to outlive AshKeyboardController.
  explicit AshKeyboardController(SessionControllerImpl* session_controller);
  ~AshKeyboardController() override;

  // Create or destroy the virtual keyboard. Called from Shell. TODO(stevenjb):
  // Fix dependencies so that the virtual keyboard can be created with the
  // keyboard controller.
  void CreateVirtualKeyboard(
      std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory);
  void DestroyVirtualKeyboard();

  // Forwards events to observers.
  void SendOnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds);
  void SendOnLoadKeyboardContentsRequested();
  void SendOnKeyboardUIDestroyed();

  // ash::KeyboardController:
  void KeyboardContentsLoaded(const gfx::Size& size) override;
  keyboard::KeyboardConfig GetKeyboardConfig() override;
  void SetKeyboardConfig(
      const keyboard::KeyboardConfig& keyboard_config) override;
  bool IsKeyboardEnabled() override;
  void SetEnableFlag(keyboard::KeyboardEnableFlag flag) override;
  void ClearEnableFlag(keyboard::KeyboardEnableFlag flag) override;
  const std::set<keyboard::KeyboardEnableFlag>& GetEnableFlags() override;
  void ReloadKeyboardIfNeeded() override;
  void RebuildKeyboardIfEnabled() override;
  bool IsKeyboardVisible() override;
  void ShowKeyboard() override;
  void HideKeyboard(HideReason reason) override;
  void SetContainerType(keyboard::ContainerType container_type,
                        const base::Optional<gfx::Rect>& target_bounds,
                        SetContainerTypeCallback callback) override;
  void SetKeyboardLocked(bool locked) override;
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override;
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override;
  void SetDraggableArea(const gfx::Rect& bounds) override;
  void AddObserver(KeyboardControllerObserver* observer) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard_controller_.get();
  }

  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }

  // Called whenever a root window is closing.
  // If the root window contains the virtual keyboard window, deactivates
  // the keyboard so that its window doesn't get destroyed as well.
  void OnRootWindowClosing(aura::Window* root_window);

 private:
  // KeyboardControllerObserver:
  void OnKeyboardConfigChanged(const keyboard::KeyboardConfig& config) override;
  void OnKeyboardVisibilityChanged(bool is_visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardEnableFlagsChanged(
      const std::set<keyboard::KeyboardEnableFlag>& flags) override;
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  SessionControllerImpl* session_controller_;  // unowned
  std::unique_ptr<keyboard::KeyboardController> keyboard_controller_;
  std::unique_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  base::ObserverList<KeyboardControllerObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardController);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
