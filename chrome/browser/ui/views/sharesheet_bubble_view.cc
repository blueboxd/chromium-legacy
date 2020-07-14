// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet_bubble_view.h"

#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

SharesheetBubbleView::SharesheetBubbleView(
    views::View* anchor_view,
    sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetAnchorView(anchor_view);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble() {
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  GetWidget()->GetRootView()->Layout();
  widget->Show();
}

void SharesheetBubbleView::CloseBubble() {
  views::Widget* widget = View::GetWidget();
  widget->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
}

void SharesheetBubbleView::OnWidgetDestroyed(views::Widget* widget) {
  delegate_->OnBubbleClosed();
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  gfx::Size size = gfx::Size(width, GetHeightForWidth(width));
  return size;
}
