// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"

#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

class OmniboxRowView::HeaderView : public views::View,
                                   public views::ButtonListener {
 public:
  explicit HeaderView(PrefService* pref_service) : pref_service_(pref_service) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));

    header_text_ = AddChildView(std::make_unique<views::Label>());
    header_text_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    layout->SetFlexForView(header_text_, 1);

    const gfx::FontList& font =
        views::style::GetFont(CONTEXT_OMNIBOX_PRIMARY,
                              views::style::STYLE_PRIMARY)
            .DeriveWithWeight(gfx::Font::Weight::MEDIUM);
    header_text_->SetFontList(font);
    header_text_->SetEnabledColor(gfx::kGoogleGrey700);

    // TODO(tommycli): Add a focus ring.
    hide_button_ = AddChildView(views::CreateVectorToggleImageButton(this));
    views::InstallCircleHighlightPathGenerator(hide_button_);
  }

  void SetHeader(int suggestion_group_id, const base::string16& header_text) {
    suggestion_group_id_ = suggestion_group_id;

    // TODO(tommycli): Our current design calls for uppercase text here, but
    // it seems like an open question what should happen for non-Latin locales.
    // Moreover, it seems unusual to do case conversion in Views in general.
    header_text_->SetText(base::i18n::ToUpper(header_text));

    if (pref_service_) {
      hide_button_->SetToggled(omnibox::IsSuggestionGroupIdHidden(
          pref_service_, suggestion_group_id_));
    }
  }

  // views::View:
  gfx::Insets GetInsets() const override {
    // Makes the header height roughly the same as the single-line row height.
    constexpr int vertical = 6;

    // Aligns the header text with the icons of ordinary matches. The assumed
    // small icon width here is lame, but necessary, since it's not explicitly
    // defined anywhere else in the code.
    constexpr int assumed_match_cell_icon_width = 16;
    constexpr int left_inset = OmniboxMatchCellView::kMarginLeft +
                               (OmniboxMatchCellView::kImageBoundsWidth -
                                assumed_match_cell_icon_width) /
                                   2;

    return gfx::Insets(vertical, left_inset, vertical,
                       OmniboxMatchCellView::kMarginRight);
  }
  void OnMouseEntered(const ui::MouseEvent& event) override {
    UpdateUIForHoverState();
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    UpdateUIForHoverState();
  }
  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    // When the theme is updated, also refresh the hover-specific UI, which is
    // all of the UI.
    UpdateUIForHoverState();
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender, hide_button_);

    if (!pref_service_)
      return;

    omnibox::ToggleSuggestionGroupIdVisibility(pref_service_,
                                               suggestion_group_id_);
    hide_button_->SetToggled(omnibox::IsSuggestionGroupIdHidden(
        pref_service_, suggestion_group_id_));
  }

 private:
  // Some UI changes on-hover, and this function effects those changes.
  void UpdateUIForHoverState() {
    OmniboxPartState part_state =
        IsMouseHovered() ? OmniboxPartState::HOVERED : OmniboxPartState::NORMAL;
    SkColor icon_color = GetOmniboxColor(GetThemeProvider(),
                                         OmniboxPart::RESULTS_ICON, part_state);
    hide_button_->set_ink_drop_base_color(icon_color);

    int dip_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
    const gfx::ImageSkia arrow_down =
        gfx::CreateVectorIcon(omnibox::kChevronIcon, dip_size, icon_color);
    const gfx::ImageSkia arrow_up =
        gfx::ImageSkiaOperations::CreateRotatedImage(
            arrow_down, SkBitmapOperations::ROTATION_180_CW);

    // The "untoggled" button state corresponds with the group being shown.
    // The "toggled" button state corresponds with the group being hidden.
    hide_button_->SetImage(views::Button::STATE_NORMAL, arrow_up);
    hide_button_->SetToggledImage(views::Button::STATE_NORMAL, &arrow_down);

    // It's a little hokey that we're stealing the logic for the background
    // color from OmniboxResultView. If we start doing this is more than just
    // one place, we should introduce a more elegant abstraction here.
    SetBackground(OmniboxResultView::GetPopupCellBackground(this, part_state));
  }

  // Non-owning pointer to the preference service used for toggling headers.
  // May be nullptr in tests.
  PrefService* const pref_service_;

  // The Label containing the header text. This is never nullptr.
  views::Label* header_text_;

  // The button used to toggle hiding suggestions with this header.
  views::ToggleImageButton* hide_button_;

  // The group ID associated with this header.
  int suggestion_group_id_ = 0;
};

OmniboxRowView::OmniboxRowView(std::unique_ptr<OmniboxResultView> result_view,
                               PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(result_view);
  DCHECK(pref_service);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  result_view_ = AddChildView(std::move(result_view));
}

void OmniboxRowView::ShowHeader(int suggestion_group_id,
                                const base::string16& header_text) {
  // Create the header (at index 0) if it doesn't exist.
  if (header_view_ == nullptr) {
    header_view_ =
        AddChildViewAt(std::make_unique<HeaderView>(pref_service_), 0);
  }

  header_view_->SetHeader(suggestion_group_id, header_text);
  header_view_->SetVisible(true);
}

void OmniboxRowView::HideHeader() {
  if (header_view_)
    header_view_->SetVisible(false);
}

gfx::Insets OmniboxRowView::GetInsets() const {
  // A visible header means this is the start of a new section. Give the section
  // that just ended an extra 4dp of padding. https://crbug.com/1076646
  if (header_view_ && header_view_->GetVisible())
    return gfx::Insets(4, 0, 0, 0);

  return gfx::Insets();
}
