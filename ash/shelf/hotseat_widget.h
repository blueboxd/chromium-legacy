// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOTSEAT_WIDGET_H_
#define ASH_SHELF_HOTSEAT_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/shelf_component.h"
#include "base/optional.h"
#include "ui/views/widget/widget.h"

namespace aura {
class ScopedWindowTargeter;
}

namespace ash {
class FocusCycler;
class ScrollableShelfView;
class Shelf;
class ShelfView;
class HotseatTransitionAnimator;

// The hotseat widget is part of the shelf and hosts app shortcuts.
class ASH_EXPORT HotseatWidget : public ShelfComponent,
                                 public ShelfConfig::Observer,
                                 public views::Widget {
 public:
  HotseatWidget();
  ~HotseatWidget() override;

  // Returns whether the hotseat background should be shown.
  static bool ShouldShowHotseatBackground();

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container, Shelf* shelf);

  // Initializes the animation metrics reporter responsible for recording
  // animation performance during hotseat state changes, and attaches
  // |delegate_view_| as an observer.
  void OnHotseatTransitionAnimatorCreated(HotseatTransitionAnimator* animator);

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnNativeWidgetActivationChanged(bool active) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // Whether the widget is in the extended position.
  bool IsExtended() const;

  // Finds the first or last focusable app shortcut and focuses it.
  void FocusFirstOrLastFocusableChild(bool last);

  // Notifies children of tablet mode state changes.
  void OnTabletModeChanged();

  // Returns the target opacity (between 0 and 1) given current conditions.
  float CalculateOpacity() const;

  // Sets the bounds of the translucent background which functions as the
  // hotseat background.
  void SetTranslucentBackground(const gfx::Rect& background_bounds);

  // Calculates the hotseat y position for |hotseat_target_state| in screen
  // coordinates.
  int CalculateHotseatYInScreen(HotseatState hotseat_target_state) const;

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

  // TODO(manucornet): Remove this method once all the hotseat layout
  // code has moved to this class.
  void set_target_bounds(gfx::Rect target_bounds) {
    target_bounds_ = target_bounds;
  }

  gfx::Size GetTranslucentBackgroundSize() const;

  // Sets the focus cycler and adds the hotseat to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);

  bool IsShowingShelfMenu() const;

  ShelfView* GetShelfView();
  const ShelfView* GetShelfView() const;

  // Returns the hotseat height (or width for side shelf).
  int GetHotseatSize() const;

  // Returns the drag distance required to fully show the hotseat widget from
  // the hidden state.
  int GetHotseatFullDragAmount() const;

  // Returns the background blur of the |translucent_background_|, for tests.
  int GetHotseatBackgroundBlurForTest() const;

  // Returns whether the translucent background is visible, for tests.
  bool GetIsTranslucentBackgroundVisibleForTest() const;

  ui::AnimationMetricsReporter* GetTranslucentBackgroundMetricsReporter();

  void SetState(HotseatState state);
  HotseatState state() const { return state_; }

  ScrollableShelfView* scrollable_shelf_view() {
    return scrollable_shelf_view_;
  }

  const ScrollableShelfView* scrollable_shelf_view() const {
    return scrollable_shelf_view_;
  }

  // Whether the widget is in the extended position because of a direct
  // manual user intervention (dragging the hotseat into its extended state).
  // This will return |false| after any visible change in the shelf
  // configuration.
  bool is_manually_extended() { return is_manually_extended_; }

  void set_manually_extended(bool value) { is_manually_extended_ = value; }

  bool is_forced_dense() const { return is_forced_dense_; }

 private:
  class DelegateView;

  struct LayoutInputs {
    gfx::Rect bounds;
    float opacity = 0.0f;
    bool is_active_session_state = false;

    bool operator==(const LayoutInputs& other) const {
      return bounds == other.bounds && opacity == other.opacity &&
             is_active_session_state == other.is_active_session_state;
    }
  };

  // Collects the inputs for layout.
  LayoutInputs GetLayoutInputs() const;

  // The set of inputs that impact this widget's layout. The assumption is that
  // this widget needs a relayout if, and only if, one or more of these has
  // changed.
  base::Optional<LayoutInputs> layout_inputs_;

  gfx::Rect target_bounds_;
  HotseatState state_ = HotseatState::kShownClamshell;

  Shelf* shelf_ = nullptr;

  // View containing the shelf items within an active user session. Owned by
  // the views hierarchy.
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;

  // The contents view of this widget. Contains |shelf_view_| and the background
  // of the hotseat.
  DelegateView* delegate_view_ = nullptr;

  // Whether the widget is currently extended because the user has manually
  // dragged it. This will be reset with any visible shelf configuration change.
  bool is_manually_extended_ = false;

  // Indicates whether app scaling is triggered, which scales shelf app icons
  // from the normal size to the dense size in tablet mode if hotseat does not
  // have enough space to show all app icons without scrolling.
  bool is_forced_dense_ = false;

  // The window targeter installed on the hotseat. Filters out events which land
  // on the non visible portion of the hotseat, or events that reach the hotseat
  // during an animation.
  std::unique_ptr<aura::ScopedWindowTargeter> hotseat_window_targeter_;

  DISALLOW_COPY_AND_ASSIGN(HotseatWidget);
};

}  // namespace ash

#endif  // ASH_SHELF_HOTSEAT_WIDGET_H_
