// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet_bubble_view.h"

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Sizes are in px.
constexpr int kButtonWidth = 92;
constexpr int kButtonHeight = 104;
constexpr int kButtonLineHeight = 20;
constexpr int kButtonPadding = 8;

constexpr int kCornerRadius = 12;
constexpr int kMaxTargetsPerRow = 4;
constexpr int kBubbleWidth = 416;
constexpr int kSpacing = 24;
constexpr int kTitleLineHeight = 24;
constexpr char kTitle[] = "Share";

constexpr SkColor kShareTitleColor = gfx::kGoogleGrey900;
constexpr SkColor kShareTargetTitleColor = gfx::kGoogleGrey700;

enum { COLUMN_SET_ID_TITLE, COLUMN_SET_ID_TARGETS };

}  // namespace

// ShareSheetTargetButton

// A button that represents a candidate share target.
class ShareSheetTargetButton : public views::Button {
 public:
  ShareSheetTargetButton(views::ButtonListener* listener,
                         const base::string16& display_name,
                         const gfx::ImageSkia* icon)
      : Button(listener) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(kButtonPadding),
        kButtonPadding, true));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* image = AddChildView(std::make_unique<views::ImageView>());
    image->set_can_process_events_within_subtree(false);

    if (!icon->isNull()) {
      image->SetImage(icon);
    }

    auto* label = AddChildView(std::make_unique<views::Label>(display_name));
    label->SetFontList(gfx::FontList("Roboto, Medium, 14px"));
    label->SetLineHeight(kButtonLineHeight);
    label->SetBackgroundColor(SK_ColorTRANSPARENT);
    label->SetEnabledColor(kShareTargetTitleColor);
    label->SetHandlesTooltips(true);
    label->SetTooltipText(display_name);
    label->SetMultiLine(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    SetFocusForPlatform();
  }

  ShareSheetTargetButton(const ShareSheetTargetButton&) = delete;
  ShareSheetTargetButton& operator=(const ShareSheetTargetButton&) = delete;

  // Button is 76px width x 88px height + 8px padding along all sides.
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kButtonWidth, kButtonHeight);
  }
};

SharesheetBubbleView::SharesheetBubbleView(
    views::View* anchor_view,
    sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  SetAnchorView(anchor_view);
  CreateBubble();
}

SharesheetBubbleView::SharesheetBubbleView(
    content::WebContents* web_contents,
    sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  // TODO(crbug.com/1097623): Make the bubble located in the center of the
  // invoke window.
  set_parent_window(web_contents->GetNativeView());
  CreateBubble();
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble(std::vector<TargetInfo> targets,
                                      apps::mojom::IntentPtr intent) {
  targets_ = std::move(targets);
  intent_ = std::move(intent);

  auto* main_layout =
      main_view_->SetLayoutManager(std::make_unique<views::GridLayout>());

  // Set up columnsets
  views::ColumnSet* cs = main_layout->AddColumnSet(COLUMN_SET_ID_TITLE);
  cs->AddColumn(/* h_align */ views::GridLayout::FILL,
                /* v_align */ views::GridLayout::LEADING,
                /* resize_percent */ 0,
                views::GridLayout::ColumnSize::kUsePreferred,
                /* fixed_width */ 0, /*min_width*/ 0);

  views::ColumnSet* cs_buttons =
      main_layout->AddColumnSet(COLUMN_SET_ID_TARGETS);
  for (int i = 0; i < kMaxTargetsPerRow; i++) {
    cs_buttons->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                          0, views::GridLayout::ColumnSize::kFixed,
                          kButtonWidth, 0);
  }

  // Add Title label
  main_layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE,
                        kTitleLineHeight);
  auto* title = main_layout->AddView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(base::StringPiece(kTitle))));
  title->SetFontList(gfx::FontList("GoogleSans, Medium, 24px"));
  title->SetLineHeight(kTitleLineHeight);
  title->SetEnabledColor(kShareTitleColor);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Add Targets
  size_t i = 0;
  for (const auto& target : targets_) {
    if (i % kMaxTargetsPerRow == 0) {
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
  root_view_->SetVisible(false);
  share_action_view_->SetVisible(true);
}

void SharesheetBubbleView::CloseBubble() {
  views::Widget* widget = View::GetWidget();
  widget->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
}

void SharesheetBubbleView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  auto type = targets_[sender->tag()].type;
  if (type == sharesheet::TargetType::kAction) {
    active_target_ = targets_[sender->tag()].launch_name;
  }
  delegate_->OnTargetSelected(targets_[sender->tag()].launch_name, type,
                              std::move(intent_), share_action_view_);
  intent_.reset();
}

std::unique_ptr<views::NonClientFrameView>
SharesheetBubbleView::CreateNonClientFrameView(views::Widget* widget) {
  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow(), color());
  bubble_border->SetCornerRadius(kCornerRadius);
  auto frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

void SharesheetBubbleView::OnWidgetDestroyed(views::Widget* widget) {
  delegate_->OnBubbleClosed(active_target_);
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize() const {
  return gfx::Size(kBubbleWidth, GetHeightForWidth(kBubbleWidth));
}

void SharesheetBubbleView::CreateBubble() {
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  set_margins(gfx::Insets());

  auto root_view = std::make_unique<views::View>();
  root_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kSpacing), 0,
      true));
  root_view_ = AddChildView(std::move(root_view));

  auto main_view = std::make_unique<views::View>();
  main_view_ = root_view_->AddChildView(std::move(main_view));

  auto share_action_view = std::make_unique<views::View>();
  share_action_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0, true));
  share_action_view_ = AddChildView(std::move(share_action_view));
  share_action_view_->SetVisible(false);
}
