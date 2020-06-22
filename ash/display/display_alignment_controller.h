// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ALIGNMENT_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_ALIGNMENT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_observer.h"
#include "base/containers/flat_map.h"
#include "ui/events/event_handler.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

class DisplayAlignmentIndicator;

// DisplayAlignmentController is responsible for creating new
// DisplayAlignmentIndicators when the activation criteria is met.
// TODO(1091497): Consider combining DisplayHighlightController and
// DisplayAlignmentController.
class ASH_EXPORT DisplayAlignmentController
    : public ui::EventHandler,
      public WindowTreeHostManager::Observer,
      public SessionObserver {
 public:
  enum class DisplayAlignmentState {
    // No indicators shown and mouse is not on edge
    kIdle,

    // Mouse is currently on one of the edges.
    kOnEdge,

    // The indicators are visible.
    kIndicatorsVisible,

    // Screen is locked or there is only one display.
    kDisabled,
  };

  DisplayAlignmentController();
  DisplayAlignmentController(const DisplayAlignmentController&) = delete;
  DisplayAlignmentController& operator=(const DisplayAlignmentController&) =
      delete;
  ~DisplayAlignmentController() override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;
  void OnDisplaysInitialized() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // Overrides the default OneShotTimer for unit testing.
  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

  const std::vector<std::unique_ptr<DisplayAlignmentIndicator>>&
  GetActiveIndicatorsForTesting();

 private:
  // Show all indicators on |src_display| and other indicators that shares
  // an edge with |src_display|. Indicators on other displays are shown without
  // pills. All indicators are created in this method and stored in
  // |active_indicators_| to be destroyed in ResetState().
  void ShowIndicators(const display::Display& src_display);

  // Clears all indicators, containers, timer, and resets the state back to
  // kIdle.
  void ResetState();

  // Used to transition to kDisable if required. Called whenever display
  // configuration or lock state updates.
  void RefreshState();

  // Stores all DisplayAlignmentIndicators currently being shown. All indicators
  // should either belong to or be a shared edge of display with
  // |triggered_display_id_|. Indicators are created upon activation in
  // ShowIndicators() and cleared in ResetState().
  std::vector<std::unique_ptr<DisplayAlignmentIndicator>> active_indicators_;

  // Timer used for both edge trigger timeouts and hiding indicators.
  std::unique_ptr<base::OneShotTimer> action_trigger_timer_;

  // Tracks current state of the controller. Mostly used to determine if action
  // is taken in OnMouseEvent();
  DisplayAlignmentState current_state_ = DisplayAlignmentState::kIdle;

  // Tracks if the screen is locked to disable highlights.
  bool is_locked_ = false;

  // Keeps track of the most recent display where the mouse hit the edge.
  // Prevents activating indicators when user hits edges of different displays.
  int64_t triggered_display_id_ = display::kInvalidDisplayId;

  // Number of times the mouse was on an edge of some display specified by
  // |triggered_display_id_| recently.
  int trigger_count_ = 0;
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ALIGNMENT_CONTROLLER_H_
