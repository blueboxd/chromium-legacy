// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"

#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/gurl.h"

SafetyTipPageInfoBubbleView::SafetyTipPageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& url,
    base::OnceCallback<void(safety_tips::SafetyTipInteraction)> close_callback)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_SAFETY_TIP,
                             web_contents),
      url_(url),
      close_callback_(std::move(close_callback)) {
  // Keep the bubble open until explicitly closed (or we navigate away, a tab is
  // created over it, etc).
  set_close_on_deactivate(false);

  const base::string16 title_text = l10n_util::GetStringUTF16(
      safety_tips::GetSafetyTipTitleId(safety_tip_status));
  set_window_title(title_text);

  views::BubbleDialogDelegateView::CreateBubble(this);

  // Replace the original title view with our formatted title.
  views::Label* original_title =
      static_cast<views::Label*>(GetBubbleFrameView()->title());
  views::StyledLabel::RangeStyleInfo name_style;
  const auto kSizeDeltaInPixels = 3;
  name_style.custom_font = original_title->GetDefaultFontList().Derive(
      kSizeDeltaInPixels, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::BOLD);
  views::StyledLabel::RangeStyleInfo base_style;
  base_style.custom_font = original_title->GetDefaultFontList().Derive(
      kSizeDeltaInPixels, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::NORMAL);

  auto new_title = std::make_unique<views::StyledLabel>(title_text, nullptr);
  new_title->AddStyleRange(gfx::Range(0, title_text.length()), name_style);
  GetBubbleFrameView()->SetTitleView(std::move(new_title));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Configure layout.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnId = 0;
  views::ColumnSet* label_col_set = layout->AddColumnSet(kColumnId);
  label_col_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                           1.0, views::GridLayout::USE_PREF, 0, 0);

  // Add text description.
  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  auto text_label = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
      safety_tips::GetSafetyTipDescriptionId(safety_tip_status)));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(
      layout_provider->GetDistanceMetric(DISTANCE_BUBBLE_PREFERRED_WIDTH));
  layout->AddView(std::move(text_label));

  // Add leave site button.
  const int hover_list_spacing = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                              views::GridLayout::kFixedSize,
                              hover_list_spacing);
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_LEAVE_BUTTON)));
  button->SetID(PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_LEAVE_SITE);
  leave_button_ =
      layout->AddView(std::move(button), 1, 1, views::GridLayout::TRAILING,
                      views::GridLayout::LEADING);

  Layout();
  SizeToContents();
}

SafetyTipPageInfoBubbleView::~SafetyTipPageInfoBubbleView() {}

void SafetyTipPageInfoBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kLostFocus:
      // We require that the user explicitly interact with the bubble, so do
      // nothing in these cases.
      break;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      // If they've left the site, we can still ignore the result; if they
      // stumble there again, we should warn again.
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
    case views::Widget::ClosedReason::kCancelButtonClicked:
      action_taken_ = safety_tips::SafetyTipInteraction::kDismiss;
      Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
      if (browser)
        safety_tips::ReputationService::Get(browser->profile())
            ->SetUserIgnore(web_contents(), url_);
      break;
  }
  std::move(close_callback_).Run(action_taken_);
}

void SafetyTipPageInfoBubbleView::ButtonPressed(views::Button* button,
                                                const ui::Event& event) {
  switch (button->GetID()) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_LEAVE_SITE:
      action_taken_ = safety_tips::SafetyTipInteraction::kLeaveSite;
      safety_tips::LeaveSite(web_contents());
      return;
  }
  NOTREACHED();
}

namespace safety_tips {

void ShowSafetyTipDialog(
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& virtual_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPageInfoAnchorConfiguration(
          browser, bubble_anchor_util::kLocationBar);
  gfx::Rect anchor_rect =
      configuration.anchor_view
          ? gfx::Rect()
          : bubble_anchor_util::GetPageInfoAnchorRect(browser);
  gfx::NativeWindow parent_window = browser->window()->GetNativeWindow();
  gfx::NativeView parent_view = platform_util::GetViewForWindow(parent_window);

  views::BubbleDialogDelegateView* bubble = new SafetyTipPageInfoBubbleView(
      configuration.anchor_view, anchor_rect, parent_view, web_contents,
      safety_tip_status, virtual_url, std::move(close_callback));

  bubble->SetHighlightedButton(configuration.highlighted_button);
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}

}  // namespace safety_tips

PageInfoBubbleViewBase* CreateSafetyTipBubbleForTesting(
    gfx::NativeView parent_view,
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& virtual_url,
    base::OnceCallback<void(safety_tips::SafetyTipInteraction)>
        close_callback) {
  return new SafetyTipPageInfoBubbleView(
      nullptr, gfx::Rect(), parent_view, web_contents, safety_tip_status,
      virtual_url, std::move(close_callback));
}
