// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/login/ui/bottom_status_indicator.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

BottomStatusIndicator::BottomStatusIndicator() {
  icon_ = new views::ImageView;
  AddChildView(icon_);

  label_ = new views::Label();
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(1));
  label_->SetSubpixelRenderingEnabled(false);
  AddChildView(label_);

  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetVisible(false);
}

void BottomStatusIndicator::SetText(const base::string16& text, SkColor color) {
  label_->SetText(text);
  label_->SetEnabledColor(color);
}

void BottomStatusIndicator::SetIcon(const gfx::VectorIcon& vector_icon,
                                    AshColorProvider::ContentLayerType type) {
  icon_->SetImage(gfx::CreateVectorIcon(
      vector_icon, AshColorProvider::Get()->GetContentLayerColor(
                       type, AshColorProvider::AshColorMode::kDark)));
}

void BottomStatusIndicator::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTooltip;
  node_data->SetName(label_->GetText());
}

}  // namespace ash
