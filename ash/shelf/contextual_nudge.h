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
  ContextualNudge(views::View* anchor, const base::string16& text);
  ~ContextualNudge() override;

  ContextualNudge(const ContextualNudge&) = delete;
  ContextualNudge& operator=(const ContextualNudge&) = delete;

  views::Label* label() { return label_; }

  // BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  ui::LayerType GetLayerType() const override;

 private:
  views::Label* label_;
};

}  // namespace ash

#endif  // ASH_SHELF_CONTEXTUAL_NUDGE_H_
