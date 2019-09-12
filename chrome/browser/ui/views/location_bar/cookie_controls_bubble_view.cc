// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls_bubble_view.h"

#include <memory>
#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

CookieControlsBubbleView* CookieControlsBubbleView::cookie_bubble_ = nullptr;

// static
void CookieControlsBubbleView::ShowBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    content::WebContents* web_contents,
    CookieControlsController* controller,
    CookieControlsController::Status status) {
  DCHECK(web_contents);
  if (cookie_bubble_)
    return;

  cookie_bubble_ =
      new CookieControlsBubbleView(anchor_view, web_contents, controller);
  cookie_bubble_->SetHighlightedButton(highlighted_button);
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(cookie_bubble_);
  controller->Update(web_contents);
  bubble_widget->Show();
}

// static
CookieControlsBubbleView* CookieControlsBubbleView::GetCookieBubble() {
  return cookie_bubble_;
}

void CookieControlsBubbleView::OnStatusChanged(
    CookieControlsController::Status status) {
  if (status_ == status)
    return;
  status_ = status;

  UpdateUi(status, blocked_cookies_ > 0);
}

void CookieControlsBubbleView::OnBlockedCookiesCountChanged(
    int blocked_cookies) {
  // The blocked cookie count changes quite frequently, so avoid unnecessary
  // UI updates and unnecessarily calling GetBlockedDomainCount() if possible.
  if (blocked_cookies_ == blocked_cookies)
    return;

  bool has_blocked_changed =
      blocked_cookies_ == -1 || (blocked_cookies_ > 0) != (blocked_cookies > 0);
  blocked_cookies_ = blocked_cookies;

  sub_title_->SetText(
      l10n_util::GetPluralStringFUTF16(IDS_COOKIE_CONTROLS_BLOCKED_MESSAGE,
                                       controller_->GetBlockedDomainCount()));

  // If this only incremented the number of blocked sites, no UI update is
  // necessary besides the |sub_title_| text.
  if (has_blocked_changed)
    UpdateUi(status_, blocked_cookies_ > 0);
}

CookieControlsBubbleView::CookieControlsBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    CookieControlsController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  observer_.Add(controller);
}

CookieControlsBubbleView::~CookieControlsBubbleView() = default;

void CookieControlsBubbleView::UpdateUi(CookieControlsController::Status status,
                                        bool has_blocked_cookies) {
  if (status == CookieControlsController::Status::kDisabled) {
    CloseBubble();
    return;
  }

  if (status == CookieControlsController::Status::kEnabled) {
    header_image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            has_blocked_cookies ? IDR_COOKIE_BLOCKING_ON_HEADER
                                : IDR_COOKIE_BLOCKING_INACTIVE_HEADER));
    title_->SetText(
        l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_DIALOG_TITLE));
    sub_title_->SetVisible(true);
    spacer_->SetVisible(has_blocked_cookies);
    turn_off_button_->SetVisible(has_blocked_cookies);
    turn_on_button_->SetVisible(false);
  } else {
    DCHECK_EQ(status, CookieControlsController::Status::kDisabledForSite);
    header_image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_COOKIE_BLOCKING_OFF_HEADER));
    title_->SetText(
        l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_DIALOG_TITLE_OFF));
    sub_title_->SetVisible(false);
    spacer_->SetVisible(true);
    turn_off_button_->SetVisible(false);
    turn_on_button_->SetVisible(true);
  }
  Layout();
  SizeToContents();
}

void CookieControlsBubbleView::CloseBubble() {
  // Widget's Close() is async, but we don't want to use cookie_bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  cookie_bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

int CookieControlsBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void CookieControlsBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  header_image_ = new views::ImageView();
  AddChildView(header_image_);

  title_ =
      new views::Label(base::string16(), views::style::CONTEXT_DIALOG_TITLE);
  title_->SetBorder(views::CreateEmptyBorder(8, 0, 0, 0));
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_->SetMultiLine(true);
  AddChildView(title_);

  sub_title_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL);
  sub_title_->SetBorder(views::CreateEmptyBorder(8, 0, 0, 0));
  sub_title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  sub_title_->SetMultiLine(true);
  AddChildView(sub_title_);

  spacer_ = new views::View();
  spacer_->SetPreferredSize(gfx::Size(0, 16));
  AddChildView(spacer_);

  auto* button_container = new views::View();
  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  turn_on_button_ =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TURN_ON_BUTTON))
          .release();
  button_container->AddChildView(turn_on_button_);

  turn_off_button_ =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TURN_OFF_BUTTON))
          .release();
  button_container->AddChildView(turn_off_button_);

  AddChildView(button_container);
}

bool CookieControlsBubbleView::ShouldShowWindowTitle() const {
  return false;
}

bool CookieControlsBubbleView::ShouldShowCloseButton() const {
  return true;
}

void CookieControlsBubbleView::WindowClosing() {
  // |cookie_bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to nullptr when it's this bubble.
  bool this_bubble = cookie_bubble_ == this;
  if (this_bubble)
    cookie_bubble_ = nullptr;

  controller_->OnBubbleUiClosing(web_contents());
}

void CookieControlsBubbleView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (sender == turn_off_button_) {
    controller_->OnCookieBlockingEnabledForSite(false);
  } else {
    DCHECK_EQ(sender, turn_on_button_);
    controller_->OnCookieBlockingEnabledForSite(true);
  }
}
