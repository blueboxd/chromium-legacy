// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
#define ASH_SHELF_HOME_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {

class AssistantOverlay;
class HomeButton;

// Controls behavior of the HomeButton, including a possible long-press
// action (for Assistant).
// Behavior is tested indirectly in HomeButtonTest and ShelfViewInkDropTest.
class HomeButtonController : public AppListControllerObserver,
                             public SessionObserver,
                             public TabletModeObserver,
                             public DefaultVoiceInteractionObserver {
 public:
  explicit HomeButtonController(HomeButton* button);
  ~HomeButtonController() override;

  // Maybe handles a gesture event based on the event and whether voice
  // interaction is available. Returns true if the event is consumed; otherwise,
  // HomeButton should pass the event along to Button to consume.
  bool MaybeHandleGestureEvent(ui::GestureEvent* event);

  // Whether voice interaction is available via long-press.
  bool IsVoiceInteractionAvailable();

  // Whether voice interaction is currently running.
  bool IsVoiceInteractionRunning();

  bool is_showing_app_list() const { return is_showing_app_list_; }

 private:
  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override;
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;

  void OnAppListShown();
  void OnAppListDismissed();

  void StartVoiceInteractionAnimation();

  // Initialize the voice interaction overlay.
  void InitializeVoiceInteractionOverlay();

  // True if the app list is currently showing for the button's display.
  // This is useful because other app_list_visible functions aren't per-display.
  bool is_showing_app_list_ = false;

  // The button that owns this controller.
  HomeButton* const button_;

  // Owned by the button's view hierarchy. Null if voice interaction is not
  // enabled.
  AssistantOverlay* assistant_overlay_ = nullptr;
  std::unique_ptr<base::OneShotTimer> assistant_animation_delay_timer_;
  std::unique_ptr<base::OneShotTimer> assistant_animation_hide_delay_timer_;
  base::TimeTicks voice_interaction_start_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(HomeButtonController);
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
