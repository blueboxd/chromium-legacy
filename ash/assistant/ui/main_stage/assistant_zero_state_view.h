// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_

#include "base/component_export.h"
#include "ui/views/view.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantZeroStateView
    : public views::View {
 public:
  AssistantZeroStateView();
  AssistantZeroStateView(const AssistantZeroStateView&) = delete;
  AssistantZeroStateView& operator=(const AssistantZeroStateView&) = delete;
  ~AssistantZeroStateView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  void InitLayout();
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_
