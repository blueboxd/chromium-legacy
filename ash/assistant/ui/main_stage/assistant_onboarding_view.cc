// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

AssistantOnboardingView::AssistantOnboardingView(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
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

void AssistantOnboardingView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

// TODO(dmblack): Implement suggestions.
// TODO(dmblack): Finalize strings.
void AssistantOnboardingView::InitLayout() {
  SetBackground(views::CreateSolidBackground(
      SkColorSetA(gfx::kPlaceholderColor, 0.2 * 0xFF)));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kUiElementHorizontalMarginDip), kSpacingDip));

  // Greeting.
  auto greeting = std::make_unique<views::Label>();
  greeting->SetAutoColorReadabilityEnabled(false);
  greeting->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  greeting->SetEnabledColor(kTextColorPrimary);
  greeting->SetFontList(assistant::ui::GetDefaultFontList());
  greeting->SetText(base::UTF8ToUTF16(base::StringPrintf(
      "Good morning %s,", delegate_->GetPrimaryUserGivenName().c_str())));
  AddChildView(std::move(greeting));

  // Description.
  auto description = std::make_unique<views::Label>();
  description->SetAutoColorReadabilityEnabled(false);
  description->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  description->SetEnabledColor(kTextColorPrimary);
  description->SetFontList(assistant::ui::GetDefaultFontList());
  description->SetMultiLine(true);
  description->SetText(base::UTF8ToUTF16(
      "I'm your Google Assistant, here to help you throughout your day!\nHere "
      "are some things you can try to get started."));
  AddChildView(std::move(description));

  // Suggestions.
  auto* suggestions = AddChildView(std::make_unique<views::View>());
  suggestions->SetPreferredSize(gfx::Size(INT_MAX, 100));
}

}  // namespace ash
