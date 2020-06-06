// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <string>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

std::string GetGreetingMessage(AssistantViewDelegate* delegate) {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  if (now.hour < 5) {
    return l10n_util::GetStringFUTF8(
        IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT,
        base::UTF8ToUTF16(delegate->GetPrimaryUserGivenName()));
  }

  if (now.hour < 12) {
    return l10n_util::GetStringFUTF8(
        IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_MORNING,
        base::UTF8ToUTF16(delegate->GetPrimaryUserGivenName()));
  }

  if (now.hour < 17) {
    return l10n_util::GetStringFUTF8(
        IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_AFTERNOON,
        base::UTF8ToUTF16(delegate->GetPrimaryUserGivenName()));
  }

  if (now.hour < 23) {
    return l10n_util::GetStringFUTF8(
        IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_EVENING,
        base::UTF8ToUTF16(delegate->GetPrimaryUserGivenName()));
  }

  return l10n_util::GetStringFUTF8(
      IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT,
      base::UTF8ToUTF16(delegate->GetPrimaryUserGivenName()));
}

}  // namespace

// AssistantOnboardingView -----------------------------------------------------

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
  greeting->SetText(base::UTF8ToUTF16(GetGreetingMessage(delegate_)));
  AddChildView(std::move(greeting));

  // Intro.
  auto intro = std::make_unique<views::Label>();
  intro->SetAutoColorReadabilityEnabled(false);
  intro->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  intro->SetEnabledColor(kTextColorPrimary);
  intro->SetFontList(assistant::ui::GetDefaultFontList());
  intro->SetMultiLine(true);
  intro->SetText(
      l10n_util::GetStringUTF16(IDS_ASSISTANT_BETTER_ONBOARDING_INTRO));
  AddChildView(std::move(intro));

  // Suggestions.
  auto* suggestions = AddChildView(std::make_unique<views::View>());
  suggestions->SetPreferredSize(gfx::Size(INT_MAX, 100));
}

}  // namespace ash
