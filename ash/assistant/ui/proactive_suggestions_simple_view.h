// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_SIMPLE_VIEW_H_
#define ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_SIMPLE_VIEW_H_

#include "ash/assistant/ui/proactive_suggestions_view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// Simple entry point for the proactive suggestions feature.
class COMPONENT_EXPORT(ASSISTANT_UI) ProactiveSuggestionsSimpleView
    : public ProactiveSuggestionsView {
 public:
  explicit ProactiveSuggestionsSimpleView(AssistantViewDelegate* delegate);
  ~ProactiveSuggestionsSimpleView() override;

  // ProactiveSuggestionsView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  void InitLayout() override;

 private:
  views::ImageButton* close_button_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsSimpleView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_SIMPLE_VIEW_H_
