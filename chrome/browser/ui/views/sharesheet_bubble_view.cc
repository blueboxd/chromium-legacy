// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet_bubble_view.h"

#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kButtonSize = 64;
constexpr int kMaxTargetRowSize = 4;
constexpr int kSpacing = 24;
constexpr char kTitle[] = "Share";

enum { COLUMN_SET_ID_TITLE, COLUMN_SET_ID_TARGETS };

}  // namespace

// ShareSheetTargetButton

// A button that represents a candidate share target.
class ShareSheetTargetButton : public views::Button {
 public:
  ShareSheetTargetButton(views::ButtonListener* listener,
                         const base::string16& display_name,
                         const gfx::Image* icon)
      : Button(listener) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    auto* image = AddChildView(std::make_unique<views::ImageView>());
    image->set_can_process_events_within_subtree(false);

    if (!icon->IsEmpty()) {
      image->SetImage(*icon->ToImageSkia());
    }

    auto* label = AddChildView(std::make_unique<views::Label>(display_name));
    label->SetBackgroundColor(SK_ColorTRANSPARENT);
    label->SetHandlesTooltips(false);
    label->SetMultiLine(true);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    SetFocusForPlatform();
  }

  ShareSheetTargetButton(const ShareSheetTargetButton&) = delete;
  ShareSheetTargetButton& operator=(const ShareSheetTargetButton&) = delete;

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kButtonSize, kButtonSize);
  }
};

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
  views::ColumnSet* cs = main_layout->AddColumnSet(COLUMN_SET_ID_TITLE);
  cs->AddColumn(/* h_align */ views::GridLayout::FILL,
                /* v_align */ views::GridLayout::CENTER,
                /* resize_percent */ 1.0,
                views::GridLayout::ColumnSize::kUsePreferred,
                /* fixed_width */ 0, /*min_width*/ 0);

  views::ColumnSet* cs_buttons =
      main_layout->AddColumnSet(COLUMN_SET_ID_TARGETS);
  cs_buttons->AddPaddingColumn(/* resize_percent */ 1, kSpacing);
  cs_buttons->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        1.0, views::GridLayout::ColumnSize::kFixed, kButtonSize,
                        0);
  cs_buttons->AddPaddingColumn(/* resize_percent */ 1, kSpacing);
  cs_buttons->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        1.0, views::GridLayout::ColumnSize::kFixed, kButtonSize,
                        0);
  cs_buttons->AddPaddingColumn(/* resize_percent */ 1, kSpacing);
  cs_buttons->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        1.0, views::GridLayout::ColumnSize::kFixed, kButtonSize,
                        0);
  cs_buttons->AddPaddingColumn(/* resize_percent */ 1, kSpacing);
  cs_buttons->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        1.0, views::GridLayout::ColumnSize::kFixed, kButtonSize,
                        0);
  cs_buttons->AddPaddingColumn(/* resize_percent */ 1, kSpacing);

  // Add Title label
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kSpacing);
  main_layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE,
                        /* height */ kSpacing);
  auto* title = main_layout->AddView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(base::StringPiece(kTitle)),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  // Add Targets
  size_t i = 0;
  for (const auto& target : targets_) {
    if (i % kMaxTargetRowSize == 0) {
      main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kSpacing);
      main_layout->StartRow(views::GridLayout::kFixedSize,
                            COLUMN_SET_ID_TARGETS);
    }
    auto target_view = std::make_unique<ShareSheetTargetButton>(
        this, target.display_name, &target.icon);
    target_view->set_tag(i++);
    main_layout->AddView(std::move(target_view));
  }
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kSpacing);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  GetWidget()->GetRootView()->Layout();
  widget->Show();
}

void SharesheetBubbleView::ShowActionView() {
  main_view_->SetVisible(false);
  share_action_view_->SetVisible(true);
}

void SharesheetBubbleView::CloseBubble() {
  views::Widget* widget = View::GetWidget();
  widget->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
}

void SharesheetBubbleView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  active_target_ = targets_[sender->tag()].launch_name;
  delegate_->OnTargetSelected(active_target_, targets_[sender->tag()].type,
                              share_action_view_);
  RequestFocus();
}

void SharesheetBubbleView::OnWidgetDestroyed(views::Widget* widget) {
  delegate_->OnBubbleClosed(active_target_);
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  gfx::Size size = gfx::Size(width, GetHeightForWidth(width));
  return size;
}
