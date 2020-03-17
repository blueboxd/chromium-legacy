// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_NAVIGATION_WIDGET_H_
#define ASH_SHELF_SHELF_NAVIGATION_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf_component.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}

namespace views {
class BoundsAnimator;
}

namespace ui {
class AnimationMetricsReporter;
}

namespace ash {
class BackButton;
class HomeButton;
class NavigationButtonAnimationMetricsReporter;
class Shelf;
class ShelfView;

// The shelf navigation widget holds the home button and (when in tablet mode)
// the back button.
class ASH_EXPORT ShelfNavigationWidget : public ShelfComponent,
                                         public ShelfConfig::Observer,
                                         public views::Widget {
 public:
  class TestApi {
   public:
    explicit TestApi(ShelfNavigationWidget* widget);
    ~TestApi();

    // Whether the home button view is visible.
    bool IsHomeButtonVisible() const;

    // Whether the back button view is visible.
    bool IsBackButtonVisible() const;

    views::BoundsAnimator* GetBoundsAnimator();

   private:
    ShelfNavigationWidget* navigation_widget_;
  };

  ShelfNavigationWidget(Shelf* shelf, ShelfView* shelf_view);
  ~ShelfNavigationWidget() override;

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container);

  // Returns the size that this widget would like to have depending on whether
  // tablet mode is on.
  gfx::Size GetIdealSize() const;

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  bool OnNativeWidgetActivationChanged(bool active) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Getter for the back button view - nullptr if the back button should not be
  // shown for the current shelf configuration.
  BackButton* GetBackButton() const;

  // Getter for the home button view - nullptr if the home button should not be
  // shown for  the current shelf configuration.
  HomeButton* GetHomeButton() const;

  // Sets whether the last focusable child (instead of the first) should be
  // focused when activating this widget.
  void SetDefaultLastFocusableChild(bool default_last_focusable_child);

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

 private:
  class Delegate;

  void UpdateButtonVisibility(
      views::View* button,
      bool visible,
      bool animate,
      ui::AnimationMetricsReporter* animation_metrics_reporter);

  Shelf* shelf_ = nullptr;
  Delegate* delegate_ = nullptr;
  gfx::Rect target_bounds_;
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  // Animation metrics reporter for back button animations. Owned by the
  // Widget to ensure it outlives the BackButton view.
  std::unique_ptr<NavigationButtonAnimationMetricsReporter>
      back_button_metrics_reporter_;

  // Animation metrics reporter for home button animations. Owned by the
  // Widget to ensure it outlives the HomeButton view.
  std::unique_ptr<NavigationButtonAnimationMetricsReporter>
      home_button_metrics_reporter_;

  DISALLOW_COPY_AND_ASSIGN(ShelfNavigationWidget);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_NAVIGATION_WIDGET_H_
