// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

class OmniboxRowView::HeaderView : public views::Label,
                                   public views::ButtonListener {
 public:
  HeaderView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    header_text_ = AddChildView(std::make_unique<views::Label>());

    // TODO(tommycli): Add a focus ring.
    hide_button_ = AddChildView(views::CreateVectorToggleImageButton(this));
    views::InstallCircleHighlightPathGenerator(hide_button_);
    hide_button_->SetVisible(false);
  }

  void SetHeader(int suggestion_group_id, const base::string16& header_text) {
    suggestion_group_id_ = suggestion_group_id;
    header_text_->SetText(header_text);
  }

  // views::View:
  gfx::Insets GetInsets() const override { return gfx::Insets(8, 16); }
  void OnMouseEntered(const ui::MouseEvent& event) override {
    UpdateUIForHoverState();
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    UpdateUIForHoverState();
  }
  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    // Since the hide button is only visible when the part is hovered, base the
    // icon color on the hover state.
    SkColor color =
        GetOmniboxColor(GetThemeProvider(), OmniboxPart::RESULTS_ICON,
                        OmniboxPartState::HOVERED);
    hide_button_->set_ink_drop_base_color(color);

    int dip_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
    const gfx::ImageSkia arrow_down =
        gfx::CreateVectorIcon(omnibox::kChevronIcon, dip_size, color);
    const gfx::ImageSkia arrow_up =
        gfx::ImageSkiaOperations::CreateRotatedImage(
            arrow_down, SkBitmapOperations::ROTATION_180_CW);

    // The "untoggled" button state corresponds with the group being shown.
    // The "toggled" button state corresponds with the group being hidden.
    hide_button_->SetImage(views::Button::STATE_NORMAL, arrow_up);
    hide_button_->SetToggledImage(views::Button::STATE_NORMAL, &arrow_down);

    // When the theme is updated, also refresh the hover-specific UI.
    UpdateUIForHoverState();
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender, hide_button_);

    // TODO(tommycli): Implement toggling the pref here.
  }

 private:
  // Some UI changes on-hover, and this function effects those changes.
  void UpdateUIForHoverState() {
    bool is_hovered = IsMouseHovered();
    hide_button_->SetVisible(is_hovered);

    OmniboxPartState part_state =
        is_hovered ? OmniboxPartState::HOVERED : OmniboxPartState::NORMAL;

    // It's a little hokey that we're stealing the logic for the background
    // color from OmniboxResultView. If we start doing this is more than just
    // one place, we should introduce a more elegant abstraction here.
    SetBackground(OmniboxResultView::GetPopupCellBackground(this, part_state));
  }

  // The Label containing the header text. This is never nullptr.
  views::Label* header_text_;

  // The button used to toggle hiding suggestions with this header.
  views::ToggleImageButton* hide_button_;

  // The group ID associated with this header.
  int suggestion_group_id_ = 0;
};

OmniboxRowView::OmniboxRowView(std::unique_ptr<OmniboxResultView> result_view) {
  DCHECK(result_view);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  result_view_ = AddChildView(std::move(result_view));
}

void OmniboxRowView::ShowHeader(int suggestion_group_id,
                                const base::string16& header_text) {
  // Create the header (at index 0) if it doesn't exist.
  if (header_view_ == nullptr)
    header_view_ = AddChildViewAt(std::make_unique<HeaderView>(), 0);

  header_view_->SetHeader(suggestion_group_id, header_text);
  header_view_->SetVisible(true);
}

void OmniboxRowView::HideHeader() {
  if (header_view_)
    header_view_->SetVisible(false);
}
