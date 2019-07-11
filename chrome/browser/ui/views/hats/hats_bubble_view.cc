// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_bubble_view.h"

#include "base/metrics/user_metrics.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/hats/hats_web_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// static
HatsBubbleView* HatsBubbleView::instance_ = nullptr;

views::BubbleDialogDelegateView* HatsBubbleView::GetHatsBubble() {
  return instance_;
}

// static
void HatsBubbleView::Show(AppMenuButton* anchor_button, Browser* browser) {
  base::RecordAction(base::UserMetricsAction("HatsBubble.Show"));

  DCHECK(anchor_button->GetWidget());
  gfx::NativeView parent_view = anchor_button->GetWidget()->GetNativeView();

  // Bubble delegate will be deleted when its window is destroyed.
  auto* bubble = new HatsBubbleView(anchor_button, browser, parent_view);
  bubble->SetHighlightedButton(anchor_button);
  bubble->GetWidget()->Show();
}

HatsBubbleView::HatsBubbleView(AppMenuButton* anchor_button,
                               Browser* browser,
                               gfx::NativeView parent_view)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      close_bubble_helper_(this, browser),
      browser_(browser) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::HATS_BUBBLE);

  set_close_on_deactivate(false);
  set_parent_window(parent_view);
  set_margins(gfx::Insets());

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(10, 20, 10, 0),
      10));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  auto message = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_HATS_BUBBLE_TEXT));
  message->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  AddChildView(std::move(message));

  views::BubbleDialogDelegateView::CreateBubble(this);

  instance_ = this;
}

HatsBubbleView::~HatsBubbleView() {}

base::string16 HatsBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_HATS_BUBBLE_TITLE);
}

base::string16 HatsBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK
             ? l10n_util::GetStringUTF16(IDS_HATS_BUBBLE_OK_LABEL)
             : l10n_util::GetStringUTF16(IDS_NO_THANKS);
}

bool HatsBubbleView::Accept() {
  HatsWebDialog::Show(browser_);
  return true;
}

bool HatsBubbleView::ShouldShowCloseButton() const {
  return true;
}

void HatsBubbleView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  instance_ = nullptr;
}
