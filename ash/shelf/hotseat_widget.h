// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOTSEAT_WIDGET_H_
#define ASH_SHELF_HOTSEAT_WIDGET_H_

#include "ash/ash_export.h"
#include "ui/views/widget/widget.h"

namespace ash {
class FocusCycler;
class ScrollableShelfView;
class Shelf;
class ShelfView;

// The hotseat widget is part of the shelf and hosts app shortcuts.
class ASH_EXPORT HotseatWidget : public views::Widget {
 public:
  HotseatWidget();

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container, Shelf* shelf);

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnNativeWidgetActivationChanged(bool active) override;

  // Whether the overflow menu/bubble is currently being shown.
  bool IsShowingOverflowBubble() const;

  // Whether the widget has been dragged to the extended position.
  bool IsDraggedToExtended() const;

  // Focuses the first or the last app shortcut inside the overflow shelf.
  // Does nothing if the overflow shelf is not currently shown.
  void FocusOverflowShelf(bool last_element);

  // Finds the first or last focusable app shortcut and focuses it.
  void FocusFirstOrLastFocusableChild(bool last);

  // Sets the focus cycler and adds the hotseat to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);

  ShelfView* GetShelfView();
  const ShelfView* GetShelfView() const;

  ScrollableShelfView* scrollable_shelf_view() {
    return scrollable_shelf_view_;
  }

 private:
  class DelegateView;

  // View containing the shelf items within an active user session. Owned by
  // the views hierarchy.
  ShelfView* shelf_view_ = nullptr;
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;

  // The contents view of this widget. Contains |shelf_view_|.
  DelegateView* delegate_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HotseatWidget);
};

}  // namespace ash

#endif  // ASH_SHELF_HOTSEAT_WIDGET_H_
