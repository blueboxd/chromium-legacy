// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_

#include "base/component_export.h"
#include "ui/views/view.h"

namespace ash {

class AssistantViewDelegate;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOnboardingView
    : public views::View {
 public:
  explicit AssistantOnboardingView(AssistantViewDelegate* delegate);
  AssistantOnboardingView(const AssistantOnboardingView&) = delete;
  AssistantOnboardingView& operator=(const AssistantOnboardingView&) = delete;
  ~AssistantOnboardingView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  void InitLayout();

  AssistantViewDelegate* const delegate_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
