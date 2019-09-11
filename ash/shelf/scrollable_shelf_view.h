// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_
#define ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/scroll_arrow_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_container_view.h"
#include "ash/shelf/shelf_view.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class FocusSearch;
}

namespace ash {

class ASH_EXPORT ScrollableShelfView : public views::AccessiblePaneView,
                                       public ShellObserver,
                                       public ShelfButtonDelegate {
 public:
  enum LayoutStrategy {
    // The arrow buttons are not shown. It means that there is enough space to
    // accommodate all of shelf icons.
    kNotShowArrowButtons,

    // Only the left arrow button is shown.
    kShowLeftArrowButton,

    // Only the right arrow button is shown.
    kShowRightArrowButton,

    // Both buttons are shown.
    kShowButtons
  };

  ScrollableShelfView(ShelfModel* model, Shelf* shelf);
  ~ScrollableShelfView() override;

  void Init();

  // Called when the focus ring for ScrollableShelfView is enabled/disabled.
  // |activated| is true when enabling the focus ring.
  void OnFocusRingActivationChanged(bool activated);

  // Scrolls to a new page of shelf icons. |forward| indicates whether the next
  // page or previous page is shown.
  void ScrollToNewPage(bool forward);

  // AccessiblePaneView:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;
  views::View* GetDefaultFocusableChild() override;

  views::View* GetShelfContainerViewForTest();

  ShelfView* shelf_view() { return shelf_view_; }

  LayoutStrategy layout_strategy_for_test() const { return layout_strategy_; }
  gfx::Vector2dF scroll_offset_for_test() const { return scroll_offset_; }

  int first_tappable_app_index() { return first_tappable_app_index_; }
  int last_tappable_app_index() { return last_tappable_app_index_; }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  // Size of the arrow button.
  static int GetArrowButtonSize();

  // Padding at the two ends of the shelf.
  static constexpr int kEndPadding = 4;

 private:
  class GradientLayerDelegate;

  enum ScrollStatus {
    // Indicates whether the gesture scrolling is across the main axis.
    // That is, whether it is scrolling vertically for bottom shelf, or
    // whether it is scrolling horizontally for left/right shelf.
    kAcrossMainAxisScroll,

    // Indicates whether the gesture scrolling is along the main axis.
    // That is, whether it is scrolling horizontally for bottom shelf, or
    // whether it is scrolling vertically for left/right shelf.
    kAlongMainAxisScroll,

    // Not in scrolling.
    kNotInScroll
  };

  // Returns the maximum scroll distance.
  int CalculateScrollUpperBound() const;

  // Returns the clamped scroll offset.
  float CalculateClampedScrollOffset(float scroll) const;

  // Creates the animation for scrolling shelf by |scroll_distance|.
  void StartShelfScrollAnimation(float scroll_distance);

  // Update the layout strategy based on the available space.
  void UpdateLayoutStrategy(int available_length);

  // Returns whether the view should adapt to RTL.
  bool ShouldAdaptToRTL() const;

  // Returns whether the app icon layout should be centering alignment.
  bool ShouldApplyDisplayCentering() const;

  Shelf* GetShelf();
  const Shelf* GetShelf() const;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  const char* GetClassName() const override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // Overridden from ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window) override;

  // Returns the padding inset. Different Padding strategies for three scenarios
  // (1) display centering alignment
  // (2) scrollable shelf centering alignment
  // (3) overflow mode
  gfx::Insets CalculateEdgePadding() const;

  // Calculates padding for display centering alignment.
  gfx::Insets CalculatePaddingForDisplayCentering() const;

  // Returns whether the received gesture event should be handled here.
  bool ShouldHandleGestures(const ui::GestureEvent& event);

  // Handles the gesture event.
  void HandleGestureEvent(ui::GestureEvent* event);

  // Handles events for scrolling the shelf. Returns whether the event has been
  // consumed.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  // Scrolls the view by distance of |x_offset| or |y_offset|. |animating|
  // indicates whether the animation displays. |x_offset| or |y_offset| has to
  // be float. Otherwise the slow gesture drag is neglected.
  void ScrollByXOffset(float x_offset, bool animating);
  void ScrollByYOffset(float y_offset, bool animating);

  // Scrolls the view to the target offset. After scrolling, |scroll_offset_| is
  // |x_dst_offset| or |y_dst_offset|. |animating| indicates whether the
  // animation shows.
  void ScrollToXOffset(float x_target_offset, bool animating);
  void ScrollToYOffset(float y_target_offset, bool animating);

  // Calculates the distance of scrolling to show a new page of shelf icons.
  // |forward| indicates whether the next page or previous page is shown.
  float CalculatePageScrollingOffset(bool forward) const;

  // Updates the gradient zone.
  void UpdateGradientZone();

  // Returns the actual scroll offset on the view's main axis. When the left
  // arrow button shows, |shelf_view_| is translated due to the change in
  // |shelf_container_view_|'s bounds. That translation offset is not included
  // in |scroll_offset_|.
  int GetActualScrollOffset() const;

  // Updates |first_tappable_app_index_| and |last_tappable_app_index_|.
  void UpdateTappableIconIndices();

  views::View* FindFirstFocusableChild();
  views::View* FindLastFocusableChild();

  LayoutStrategy layout_strategy_ = kNotShowArrowButtons;

  // Child views Owned by views hierarchy.
  ScrollArrowView* left_arrow_ = nullptr;
  ScrollArrowView* right_arrow_ = nullptr;
  ShelfContainerView* shelf_container_view_ = nullptr;

  // Available space to accommodate shelf icons.
  int space_for_icons_ = 0;

  ShelfView* shelf_view_ = nullptr;

  gfx::Vector2dF scroll_offset_;

  ScrollStatus scroll_status_ = kNotInScroll;

  // Gesture states are preserved when the gesture scrolling along the main axis
  // (that is, whether it is scrolling horizontally for bottom shelf, or whether
  // it is scrolling horizontally for left/right shelf) gets started. They help
  // to handle the gesture fling event.
  gfx::Vector2dF scroll_offset_before_main_axis_scrolling_;
  LayoutStrategy layout_strategy_before_main_axis_scrolling_ =
      kNotShowArrowButtons;

  std::unique_ptr<GradientLayerDelegate> gradient_layer_delegate_;

  std::unique_ptr<views::FocusSearch> focus_search_;

  // The index of the first/last tappable app index.
  int first_tappable_app_index_ = -1;
  int last_tappable_app_index_ = -1;

  // Whether this view should focus its last focusable child (instead of its
  // first) when focused.
  bool default_last_focusable_child_ = false;

  // Indicates whether the focus ring on shelf items contained by
  // ScrollableShelfView is enabled.
  bool focus_ring_activated_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_
