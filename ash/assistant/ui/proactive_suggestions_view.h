// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_
#define ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class AssistantViewDelegate;

// TODO(dmblack): Replace placeholder text with live text.
// The view for proactive suggestions which serves as the feature's entry point.
class COMPONENT_EXPORT(ASSISTANT_UI) ProactiveSuggestionsView
    : public views::WidgetDelegateView,
      public AssistantUiModelObserver {
 public:
  explicit ProactiveSuggestionsView(AssistantViewDelegate* delegate);
  ~ProactiveSuggestionsView() override;

  // views::WidgetDelegateView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // AssistantUiModelObserver:
  void OnUsableWorkAreaChanged(const gfx::Rect& usable_work_area) override;

 private:
  void InitLayout();
  void InitWidget();
  void UpdateBounds();

  AssistantViewDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_
