// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"

#include <memory>

#include "ash/assistant/model/ui/assistant_text_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

// AssistantTextElementView ----------------------------------------------------

AssistantTextElementView::AssistantTextElementView(
    const AssistantTextElement* text_element) {
  InitLayout(text_element);
}

AssistantTextElementView::~AssistantTextElementView() = default;

const char* AssistantTextElementView::GetClassName() const {
  return "AssistantTextElementView";
}

ui::Layer* AssistantTextElementView::GetLayerForAnimating() {
  if (!layer()) {
    // We'll be animating this view on its own layer so we need to initialize
    // the layer for the view if we haven't done so already.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }
  return layer();
}

std::string AssistantTextElementView::ToStringForTesting() const {
  return base::UTF16ToUTF8(label_->GetText());
}

void AssistantTextElementView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantTextElementView::InitLayout(
    const AssistantTextElement* text_element) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Label.
  label_ = AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(text_element->text())));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  label_->SetEnabledColor(kTextColorPrimary);
  label_->SetFontList(assistant::ui::GetDefaultFontList()
                          .DeriveWithSizeDelta(2)
                          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetMultiLine(true);
}

}  // namespace ash
