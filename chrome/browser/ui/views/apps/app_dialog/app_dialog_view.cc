// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

AppDialogView::AppDialogView()
    : BubbleDialogDelegateView(nullptr, views::BubbleBorder::NONE) {}

AppDialogView::~AppDialogView() = default;

gfx::Size AppDialogView::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                            margins().width();
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

ui::ModalType AppDialogView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

bool AppDialogView::ShouldShowCloseButton() const {
  return false;
}

void AppDialogView::InitializeView(const gfx::ImageSkia& image,
                                   const base::string16& heading_text) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_OK);
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  auto* icon_view = AddChildView(std::make_unique<views::ImageView>());
  icon_view->SetImage(image);

  auto* label = AddChildView(std::make_unique<views::Label>(heading_text));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}
