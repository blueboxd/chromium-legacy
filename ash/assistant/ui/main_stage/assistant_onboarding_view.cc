// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include "ash/assistant/ui/assistant_view_ids.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"

namespace ash {

AssistantOnboardingView::AssistantOnboardingView() {
  SetID(AssistantViewID::kOnboardingView);
  InitLayout();
}

AssistantOnboardingView::~AssistantOnboardingView() = default;

const char* AssistantOnboardingView::GetClassName() const {
  return "AssistantOnboardingView";
}

gfx::Size AssistantOnboardingView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

// TODO(dmblack): Remove after implementing layout.
int AssistantOnboardingView::GetHeightForWidth(int width) const {
  return 100;
}

// TODO(dmblack): Implement layout.
void AssistantOnboardingView::InitLayout() {
  SetBackground(views::CreateSolidBackground(
      SkColorSetA(gfx::kPlaceholderColor, 0.2 * 0xFF)));
}

}  // namespace ash
