// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

using chromeos::assistant::AssistantSuggestion;
using chromeos::assistant::AssistantSuggestionType;

constexpr int kHorizontalMarginDip = 56;

// Greeting.
constexpr int kGreetingLabelLineHeight = 24;
constexpr int kGreetingLabelSizeDelta = 8;

// Intro.
constexpr int kIntroLabelLineHeight = 22;
constexpr int kIntroLabelMarginTopDip = 12;
constexpr int kIntroLabelSizeDelta = 3;

// Suggestions.
constexpr int kSuggestionsColumnCount = 3;
constexpr int kSuggestionsColumnSetId = 1;
constexpr int kSuggestionsCornerRadiusDip = 16;
constexpr int kSuggestionsIconSizeDip = 24;
constexpr int kSuggestionsLabelLineHeight = 20;
constexpr int kSuggestionsLabelSizeDelta = 2;
constexpr int kSuggestionsMaxCount = 6;
constexpr int kSuggestionsMarginDip = 16;
constexpr int kSuggestionsMarginTopDip = 48;
constexpr int kSuggestionsPaddingDip = 16;
constexpr int kSuggestionsPreferredHeightDip = 72;
constexpr int kSuggestionsSpacingDip = 12;

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

SkColor GetSuggestionBackgroundColor(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kSuggestionsMaxCount);
  constexpr SkColor background_colors[kSuggestionsMaxCount] = {
      gfx::kGoogleBlue050,   gfx::kGoogleYellow050, gfx::kGoogleGreen050,
      gfx::kGoogleYellow050, gfx::kGoogleGreen050,  gfx::kGoogleRed050};
  return background_colors[index];
}

SkColor GetSuggestionTextColor(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kSuggestionsMaxCount);
  constexpr SkColor text_colors[kSuggestionsMaxCount] = {
      gfx::kGoogleBlue700,   gfx::kGoogleYellow900, gfx::kGoogleGreen800,
      gfx::kGoogleYellow900, gfx::kGoogleGreen800,  gfx::kGoogleRed800};
  return text_colors[index];
}

// SuggestionView --------------------------------------------------------------

class SuggestionView : public views::Button, public views::ButtonListener {
 public:
  SuggestionView(AssistantViewDelegate* delegate,
                 const AssistantSuggestion& suggestion,
                 int index)
      : views::Button(this),
        delegate_(delegate),
        suggestion_id_(suggestion.id),
        index_(index) {
    InitLayout(suggestion);
  }

  SuggestionView(const SuggestionView&) = delete;
  SuggestionView& operator=(const SuggestionView&) = delete;
  ~SuggestionView() override = default;

  // views::Button:
  const char* GetClassName() const override { return "SuggestionView"; }

  int GetHeightForWidth(int width) const override {
    return kSuggestionsPreferredHeightDip;
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    delegate_->OnSuggestionPressed(suggestion_id_);
  }

 private:
  void InitLayout(const AssistantSuggestion& suggestion) {
    // Background.
    SetBackground(views::CreateRoundedRectBackground(
        GetSuggestionBackgroundColor(index_), kSuggestionsCornerRadiusDip));

    // Layout.
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetCollapseMargins(true)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey, views::FlexSpecification())
        .SetDefault(views::kMarginsKey, gfx::Insets(0, kSuggestionsSpacingDip))
        .SetInteriorMargin(gfx::Insets(0, kSuggestionsPaddingDip))
        .SetOrientation(views::LayoutOrientation::kHorizontal);

    // Icon.
    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetImageSize({kSuggestionsIconSizeDip, kSuggestionsIconSizeDip});
    icon_->SetPreferredSize({kSuggestionsIconSizeDip, kSuggestionsIconSizeDip});

    if (suggestion.icon_url.is_valid()) {
      delegate_->DownloadImage(suggestion.icon_url,
                               base::BindOnce(&SuggestionView::OnIconDownloaded,
                                              weak_factory_.GetWeakPtr()));
    }

    // Label.
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetEnabledColor(GetSuggestionTextColor(index_));
    label_->SetFontList(assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(
        kSuggestionsLabelSizeDelta));
    label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    label_->SetLineHeight(kSuggestionsLabelLineHeight);
    label_->SetMaxLines(2);
    label_->SetMultiLine(true);
    label_->SetPreferredSize(gfx::Size(INT_MAX, INT_MAX));
    label_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded,
                                 /*adjust_height_for_width=*/true));
    label_->SetText(base::UTF8ToUTF16(suggestion.text));
  }

  void OnIconDownloaded(const gfx::ImageSkia& icon) {
    if (!icon.isNull())
      icon_->SetImage(icon);
  }

  AssistantViewDelegate* const delegate_;
  const base::UnguessableToken suggestion_id_;
  const int index_;

  views::ImageView* icon_;  // Owned by view hierarchy.
  views::Label* label_;     // Owned by view hierarchy.

  base::WeakPtrFactory<SuggestionView> weak_factory_{this};
};

}  // namespace

// AssistantOnboardingView -----------------------------------------------------

AssistantOnboardingView::AssistantOnboardingView(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kOnboardingView);
  InitLayout();

  assistant_controller_observer_.Add(AssistantController::Get());
  AssistantSuggestionsController::Get()->GetModel()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AssistantOnboardingView::~AssistantOnboardingView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);

  if (AssistantSuggestionsController::Get())
    AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
}

const char* AssistantOnboardingView::GetClassName() const {
  return "AssistantOnboardingView";
}

gfx::Size AssistantOnboardingView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantOnboardingView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantOnboardingView::OnAssistantControllerDestroying() {
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
  assistant_controller_observer_.Remove(AssistantController::Get());
}

void AssistantOnboardingView::OnOnboardingSuggestionsChanged(
    const std::vector<AssistantSuggestion>& onboarding_suggestions) {
  UpdateSuggestions();
}

void AssistantOnboardingView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (new_visibility == AssistantVisibility::kVisible)
    UpdateGreeting();
}

void AssistantOnboardingView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kHorizontalMarginDip)));

  // Greeting.
  greeting_ = AddChildView(std::make_unique<views::Label>());
  greeting_->SetAutoColorReadabilityEnabled(false);
  greeting_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  greeting_->SetEnabledColor(SK_ColorBLACK);
  greeting_->SetFontList(assistant::ui::GetDefaultFontList()
                             .DeriveWithSizeDelta(kGreetingLabelSizeDelta)
                             .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  greeting_->SetLineHeight(kGreetingLabelLineHeight);
  greeting_->SetText(base::UTF8ToUTF16(GetGreetingMessage(delegate_)));

  // Intro.
  auto intro = std::make_unique<views::Label>();
  intro->SetAutoColorReadabilityEnabled(false);
  intro->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  intro->SetBorder(views::CreateEmptyBorder(kIntroLabelMarginTopDip, 0, 0, 0));
  intro->SetEnabledColor(gfx::kGoogleGrey900);
  intro->SetFontList(assistant::ui::GetDefaultFontList()
                         .DeriveWithSizeDelta(kIntroLabelSizeDelta)
                         .DeriveWithWeight(gfx::Font ::Weight::MEDIUM));
  intro->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  intro->SetLineHeight(kIntroLabelLineHeight);
  intro->SetMultiLine(true);
  intro->SetText(
      l10n_util::GetStringUTF16(IDS_ASSISTANT_BETTER_ONBOARDING_INTRO));
  AddChildView(std::move(intro));

  // Suggestions.
  UpdateSuggestions();
}

void AssistantOnboardingView::UpdateSuggestions() {
  if (grid_)
    RemoveChildViewT(grid_);

  grid_ = AddChildView(std::make_unique<views::View>());
  grid_->SetBorder(views::CreateEmptyBorder(kSuggestionsMarginTopDip, 0, 0, 0));

  auto* layout = grid_->SetLayoutManager(std::make_unique<views::GridLayout>());
  auto* columns = layout->AddColumnSet(kSuggestionsColumnSetId);

  // Initialize columns.
  for (int i = 0; i < kSuggestionsColumnCount; ++i) {
    if (i > 0) {
      columns->AddPaddingColumn(
          /*resize_percent=*/views::GridLayout::kFixedSize,
          /*width=*/kSuggestionsMarginDip);
    }
    columns->AddColumn(
        /*h_align=*/views::GridLayout::Alignment::FILL,
        /*v_align=*/views::GridLayout::Alignment::FILL, /*resize_percent=*/1.0,
        /*size_type=*/views::GridLayout::ColumnSize::kFixed,
        /*fixed_width=*/0, /*min_width=*/0);
  }

  const std::vector<AssistantSuggestion>& suggestions =
      AssistantSuggestionsController::Get()
          ->GetModel()
          ->GetOnboardingSuggestions();

  // Initialize suggestions.
  for (size_t i = 0; i < suggestions.size() && i < kSuggestionsMaxCount; ++i) {
    if (i % kSuggestionsColumnCount == 0) {
      if (i > 0) {
        layout->StartRowWithPadding(
            /*vertical_resize=*/views::GridLayout::kFixedSize,
            /*column_set_id=*/kSuggestionsColumnSetId,
            /*padding_resize=*/views::GridLayout::kFixedSize,
            /*padding=*/kSuggestionsMarginDip);
      } else {
        layout->StartRow(/*vertical_resize=*/views::GridLayout::kFixedSize,
                         /*column_set_id=*/kSuggestionsColumnSetId);
      }
    }
    layout->AddView(
        std::make_unique<SuggestionView>(delegate_, suggestions.at(i), i));
  }
}

void AssistantOnboardingView::UpdateGreeting() {
  greeting_->SetText(base::UTF8ToUTF16(GetGreetingMessage(delegate_)));
}

}  // namespace ash
