// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"

#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

AssistantZeroStateView::AssistantZeroStateView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kZeroStateView);
  InitLayout();
}

AssistantZeroStateView::~AssistantZeroStateView() = default;

const char* AssistantZeroStateView::GetClassName() const {
  return "AssistantZeroStateView";
}

gfx::Size AssistantZeroStateView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantZeroStateView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

// TODO(dmblack): Update conditions under which onboarding view is shown.
void AssistantZeroStateView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Onboarding.
  if (chromeos::assistant::features::IsBetterOnboardingEnabled()) {
    AddChildView(std::make_unique<AssistantOnboardingView>(delegate_));
    return;
  }

  // Greeting label.
  auto greeting_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
  greeting_label->SetID(AssistantViewID::kGreetingLabel);
  greeting_label->SetAutoColorReadabilityEnabled(false);
  greeting_label->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  greeting_label->SetEnabledColor(kTextColorPrimary);
  greeting_label->SetFontList(assistant::ui::GetDefaultFontList()
                                  .DeriveWithSizeDelta(8)
                                  .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  greeting_label->SetMultiLine(true);
  AddChildView(std::move(greeting_label));
}

}  // namespace ash
