// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/proactive_suggestions_view.h"

#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
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
constexpr int kCloseButtonSizeDip = 16;
constexpr int kLineHeightDip = 20;
constexpr int kMaxWidthDip = 240;
constexpr int kPreferredHeightDip = 32;

}  // namespace

ProactiveSuggestionsView::ProactiveSuggestionsView(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();
  InitWidget();

  delegate_->AddUiModelObserver(this);
}

ProactiveSuggestionsView::~ProactiveSuggestionsView() {
  delegate_->RemoveUiModelObserver(this);
}

const char* ProactiveSuggestionsView::GetClassName() const {
  return "ProactiveSuggestionsView";
}

gfx::Size ProactiveSuggestionsView::CalculatePreferredSize() const {
  int preferred_width = views::View::CalculatePreferredSize().width();
  preferred_width = std::min(preferred_width, kMaxWidthDip);
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int ProactiveSuggestionsView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void ProactiveSuggestionsView::OnPaintBackground(gfx::Canvas* canvas) {
  const int radius = height() / 2;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(gfx::kGoogleGrey800);
  flags.setDither(true);

  canvas->DrawRoundRect(GetLocalBounds(), radius, flags);
}

void ProactiveSuggestionsView::OnUsableWorkAreaChanged(
    const gfx::Rect& usable_work_area) {
  UpdateBounds();
}

void ProactiveSuggestionsView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kSpacingDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Assistant icon.
  views::ImageView* assistant_icon = new views::ImageView();
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      ash::kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
  assistant_icon->SetPreferredSize(
      gfx::Size(kAssistantIconSizeDip, kAssistantIconSizeDip));
  AddChildView(assistant_icon);

  // Label.
  views::Label* label = new views::Label();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  label->SetEnabledColor(gfx::kGoogleGrey100);
  label->SetFontList(
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(1));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetLineHeight(kLineHeightDip);
  label->SetMultiLine(false);
  label->SetText(base::UTF8ToUTF16("Placeholder text"));
  AddChildView(label);

  // Close button.
  views::ImageButton* close_button = new views::ImageButton(nullptr);
  close_button->SetImage(
      views::ImageButton::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseButtonSizeDip,
                            gfx::kGoogleGrey100));
  close_button->SetPreferredSize(
      gfx::Size(kCloseButtonSizeDip, kCloseButtonSizeDip));
  AddChildView(close_button);
}

void ProactiveSuggestionsView::InitWidget() {
  views::Widget::InitParams params;
  params.context = delegate_->GetRootWindowForNewWindows();
  params.delegate = this;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));

  UpdateBounds();
}

void ProactiveSuggestionsView::UpdateBounds() {
  const gfx::Rect& usable_work_area =
      delegate_->GetUiModel()->usable_work_area();

  const gfx::Size size = CalculatePreferredSize();

  const int left = usable_work_area.x();
  const int top = usable_work_area.bottom() - size.height();

  GetWidget()->SetBounds(gfx::Rect(left, top, size.width(), size.height()));
}

}  // namespace ash
