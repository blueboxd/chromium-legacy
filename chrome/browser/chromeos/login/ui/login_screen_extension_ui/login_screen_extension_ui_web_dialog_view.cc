// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_web_dialog_view.h"

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_dialog_delegate.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

LoginScreenExtensionUiWebDialogView::LoginScreenExtensionUiWebDialogView(
    content::BrowserContext* context,
    LoginScreenExtensionUiDialogDelegate* delegate,
    std::unique_ptr<ui::WebDialogWebContentsDelegate::WebContentsHandler>
        handler)
    : views::WebDialogView(context, delegate, std::move(handler)),
      delegate_(delegate) {}

LoginScreenExtensionUiWebDialogView::~LoginScreenExtensionUiWebDialogView() =
    default;

bool LoginScreenExtensionUiWebDialogView::ShouldShowCloseButton() const {
  return !delegate_ || delegate_->CanCloseDialog();
}

}  // namespace chromeos
