// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_CONTEXTUAL_NUDGE_H_
#define ASH_SHELF_CONTEXTUAL_NUDGE_H_

#include "ash/ash_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// The implementation of contextual nudge tooltip bubbles.
class ASH_EXPORT ContextualNudge : public views::BubbleDialogDelegateView {
 public:
  // Indicates whether the nudge should be shown below or above the anchor.
  enum class Position { kBottom, kTop };

  // |anchor| - The view to which the nudge bubble should be anchored. May be
  //     nullptr, in which case anchor bounds should be provided using
  //     UpdateAnchorRect().
  // |parent_window| - if set, the window that should parent the nudge native
  //     window. If not set, the shelf container in the anchor view's root
  //     window will be used.
  // |text| - The nudge text.
  // |position| - The nudge position relative to the anchor rectangle.
  ContextualNudge(views::View* anchor,
                  aura::Window* parent_window,
                  const base::string16& text,
                  Position position);
  ~ContextualNudge() override;

  ContextualNudge(const ContextualNudge&) = delete;
  ContextualNudge& operator=(const ContextualNudge&) = delete;

  views::Label* label() { return label_; }

  // Sets the nudge bubble anchor rect - should be used to set the anchor rect
  // if no valid anchor was passed to the nudge bubble.
  void UpdateAnchorRect(const gfx::Rect& rect);

  // BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  ui::LayerType GetLayerType() const override;

 private:
  views::Label* label_;
};

}  // namespace ash

#endif  // ASH_SHELF_CONTEXTUAL_NUDGE_H_
