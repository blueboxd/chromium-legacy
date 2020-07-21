// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet_bubble_view.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kColumnSetIdTitle = 0;
constexpr int kVerticalSpacing = 24;
constexpr char kTitle[] = "Share";

}  // namespace

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

  auto root_view = std::make_unique<views::View>();
  root_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  root_view_ = AddChildView(std::move(root_view));

  auto main_view = std::make_unique<views::View>();
  main_view_ = root_view_->AddChildView(std::move(main_view));

  auto share_action_view = std::make_unique<views::View>();
  share_action_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  share_action_view_ = root_view_->AddChildView(std::move(share_action_view));
  share_action_view_->SetVisible(false);
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble(std::vector<TargetInfo> targets) {
  targets_ = std::move(targets);

  auto* main_layout =
      main_view_->SetLayoutManager(std::make_unique<views::GridLayout>());

  // Set up columnsets
  views::ColumnSet* cs = main_layout->AddColumnSet(kColumnSetIdTitle);
  cs->AddColumn(/* h_align */ views::GridLayout::FILL,
                /* v_align */ views::GridLayout::CENTER,
                /* resize_percent */ 1.0,
                views::GridLayout::ColumnSize::kUsePreferred,
                /* fixed_width */ 0, /*min_width*/ 0);

  // Add Title label
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kVerticalSpacing);
  main_layout->StartRow(views::GridLayout::kFixedSize, kColumnSetIdTitle,
                        /* height */ kVerticalSpacing);
  auto* title = main_layout->AddView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(base::StringPiece(kTitle)),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);

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
