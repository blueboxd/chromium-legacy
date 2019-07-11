// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_bubble.h"
#include "base/macros.h"

namespace ash {
class Shelf;
class ShelfView;

// OverflowBubbleView hosts a ShelfView to display overflown items.
// Exports to access this class from OverflowBubbleViewTestAPI.
class ASH_EXPORT OverflowBubbleView : public ShelfBubble {
 public:
  enum LayoutStrategy {
    // The arrow buttons are not shown. It means that there is enough space to
    // accommodate all of shelf icons.
    NOT_SHOW_ARROW_BUTTON,

    // Only the left arrow button is shown.
    SHOW_LEFT_ARROW_BUTTON,

    // Only the right arrow button is shown.
    SHOW_RIGHT_ARROW_BUTTON,

    // Both buttons are shown.
    SHOW_BUTTONS
  };

  // |anchor| is the overflow button on the main shelf. |shelf_view| is the
  // ShelfView containing the overflow items.
  OverflowBubbleView(ShelfView* shelf_view,
                     views::View* anchor,
                     SkColor background_color);
  ~OverflowBubbleView() override;

  // Handles events for scrolling the bubble. Returns whether the event
  // has been consumed.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  // These return the actual offset (sometimes reduced by the clamping).
  int ScrollByXOffset(int x_offset);
  int ScrollByYOffset(int y_offset);

  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  bool CanActivate() const override;

  ShelfView* shelf_view() { return shelf_container_view_->shelf_view(); }
  View* left_arrow() { return left_arrow_; }
  View* right_arrow() { return right_arrow_; }
  LayoutStrategy layout_strategy() { return layout_strategy_; }

  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

 private:
  friend class OverflowBubbleViewTestAPI;

  class ScrollArrowView : public views::View {
   public:
    enum ArrowType { LEFT, RIGHT };
    ScrollArrowView(ArrowType arrow_type, bool is_horizontal_alignment);
    ~ScrollArrowView() override = default;

    // Overridden from views::View:
    void OnPaint(gfx::Canvas* canvas) override;
    const char* GetClassName() const override;

   private:
    ArrowType arrow_type_;
    bool is_horizontal_alignment_;
  };

  class OverflowShelfContainerView : public views::View {
   public:
    explicit OverflowShelfContainerView(ShelfView* shelf_view);
    ~OverflowShelfContainerView() override = default;

    void Initialize();

    // Translate |shelf_view_| by |offset|.
    void TranslateShelfView(const gfx::Vector2d& offset);

    // views::View:
    gfx::Size CalculatePreferredSize() const override;
    void ChildPreferredSizeChanged(views::View* child) override;
    void Layout() override;
    const char* GetClassName() const override;

    ShelfView* shelf_view() { return shelf_view_; }

   private:
    ShelfView* shelf_view_;
  };

  // Returns the maximum scroll distance.
  int CalculateScrollUpperBound() const;

  // Updates the overflow bubble view's layout strategy after scrolling by the
  // distance of |scroll|. Returns the adapted scroll offset.
  int CalculateLayoutStrategyAfterScroll(int scroll);

  // Ensures that the width of |bubble_bounds| (if it is not horizontally
  // aligned, adjust |bubble_bounds|'s height) is the multiple of the sum
  // between kShelfButtonSize and kShelfButtonSpacing. It helps that all of
  // shelf icons are fully visible.
  void AdjustToEnsureIconsFullyVisible(gfx::Rect* bubble_bounds) const;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  const char* GetClassName() const override;

  // ui::EventHandler:
  void OnScrollEvent(ui::ScrollEvent* event) override;

  mutable LayoutStrategy layout_strategy_;

  // Child views Owned by views hierarchy.
  ScrollArrowView* left_arrow_ = nullptr;
  ScrollArrowView* right_arrow_ = nullptr;
  OverflowShelfContainerView* shelf_container_view_ = nullptr;

  Shelf* shelf_;
  gfx::Vector2d scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleView);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_
