// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_

#include "base/component_export.h"
#include "ui/views/view.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOnboardingView
    : public views::View {
 public:
  AssistantOnboardingView();
  AssistantOnboardingView(const AssistantOnboardingView&) = delete;
  AssistantOnboardingView& operator=(const AssistantOnboardingView&) = delete;
  ~AssistantOnboardingView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

 private:
  void InitLayout();
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
