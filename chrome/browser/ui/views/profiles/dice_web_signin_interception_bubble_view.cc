// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

namespace {
constexpr int kInterceptionBubbleHeight = 362;
constexpr int kInterceptionBubbleWidth = 290;
}  // namespace

DiceWebSigninInterceptionBubbleView::~DiceWebSigninInterceptionBubbleView() =
    default;

// static
void DiceWebSigninInterceptionBubbleView::CreateBubble(
    content::BrowserContext* browser_context,
    views::View* anchor_view) {
  // The widget is owned by the views system.
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      new DiceWebSigninInterceptionBubbleView(browser_context, anchor_view));
  // TODO(droger): Delay showing the bubble until the web view is loaded.
  widget->Show();
}

DiceWebSigninInterceptionBubbleView::DiceWebSigninInterceptionBubbleView(
    content::BrowserContext* browser_context,
    views::View* anchor_view)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT) {
  DCHECK(browser_context);

  // Create the web view in the native bubble.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(browser_context);
  web_view->LoadInitialURL(GURL(chrome::kChromeUIDiceWebSigninInterceptURL));
  web_view->SetPreferredSize(
      gfx::Size(kInterceptionBubbleWidth, kInterceptionBubbleHeight));
  AddChildView(std::move(web_view));

  set_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

// DiceWebSigninInterceptorDelegate --------------------------------------------

void DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubbleInternal(
    Browser* browser) {
  DCHECK(browser);
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAvatarToolbarButton();
  DCHECK(anchor_view);
  DiceWebSigninInterceptionBubbleView::CreateBubble(browser->profile(),
                                                    anchor_view);
}
