// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/proactive_suggestions_simple_view.h"

#include <memory>
#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "net/base/escape.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

// Appearance.
constexpr int kAssistantIconSizeDip = 16;
constexpr int kCloseButtonIconSizeDip = 16;
constexpr int kCloseButtonSizeDip = 32;
constexpr int kLineHeightDip = 20;
constexpr int kPaddingLeftDip = 8;
constexpr int kPaddingRightDip = 0;
constexpr int kPreferredHeightDip = 32;

}  // namespace

ProactiveSuggestionsSimpleView::ProactiveSuggestionsSimpleView(
    AssistantViewDelegate* delegate)
    : ProactiveSuggestionsView(delegate) {}

ProactiveSuggestionsSimpleView::~ProactiveSuggestionsSimpleView() = default;

const char* ProactiveSuggestionsSimpleView::GetClassName() const {
  return "ProactiveSuggestionsSimpleView";
}

gfx::Size ProactiveSuggestionsSimpleView::CalculatePreferredSize() const {
  const int preferred_width = std::min(
      ProactiveSuggestionsView::CalculatePreferredSize().width(),
      chromeos::assistant::features::GetProactiveSuggestionsMaxWidth());
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int ProactiveSuggestionsSimpleView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void ProactiveSuggestionsSimpleView::OnPaintBackground(gfx::Canvas* canvas) {
  const int radius = height() / 2;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(gfx::kGoogleGrey800);
  flags.setDither(true);

  canvas->DrawRoundRect(GetLocalBounds(), radius, flags);
}

void ProactiveSuggestionsSimpleView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  // There are two possible |senders|, the close button...
  if (sender == close_button_) {
    delegate()->OnProactiveSuggestionsCloseButtonPressed();
    return;
  }

  // ...and the proactive suggestions view itself.
  DCHECK_EQ(this, sender);
  delegate()->OnProactiveSuggestionsViewPressed();
}

void ProactiveSuggestionsSimpleView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingLeftDip, 0, kPaddingRightDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Assistant icon.
  auto assistant_icon = std::make_unique<views::ImageView>();
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
  assistant_icon->SetPreferredSize(
      gfx::Size(kAssistantIconSizeDip, kAssistantIconSizeDip));
  AddChildView(std::move(assistant_icon));

  // Spacing.
  // Note that we don't add similar spacing between |label_| and the
  // |close_button_| as the latter has internal spacing between its icon and
  // outer bounds so as to provide a larger hit rect to the user.
  auto spacing = std::make_unique<views::View>();
  spacing->SetPreferredSize(gfx::Size(kSpacingDip, kPreferredHeightDip));
  AddChildView(std::move(spacing));

  // Label.
  auto label = std::make_unique<views::Label>();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  label->SetEnabledColor(gfx::kGoogleGrey100);
  label->SetFontList(
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(1));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetLineHeight(kLineHeightDip);
  label->SetMultiLine(false);

  // The |description| string coming from the proactive suggestions server may
  // be HTML escaped so we need to unescape before displaying to avoid printing
  // HTML entities to the user.
  label->SetText(net::UnescapeForHTML(
      base::UTF8ToUTF16(proactive_suggestions()->description())));

  // We impose a maximum width restriction on the proactive suggestions view.
  // When restricting width, |label| should cede layout space to its siblings.
  layout_manager->SetFlexForView(AddChildView(std::move(label)), 1);

  // Close button.
  auto close_button = std::make_unique<views::ImageButton>(/*listener=*/this);
  close_button->SetImage(
      views::ImageButton::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseButtonIconSizeDip,
                            gfx::kGoogleGrey100));
  close_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  close_button->SetPreferredSize(
      gfx::Size(kCloseButtonSizeDip, kCloseButtonSizeDip));
  close_button_ = AddChildView(std::move(close_button));
}

}  // namespace ash
