// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/safety_tip_infobar_delegate.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_infobar.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using safety_tips::SafetyTipInfoBarDelegate;

namespace safety_tips {

// From safety_tip_ui.h
void ShowSafetyTipDialog(
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  auto delegate = std::make_unique<SafetyTipInfoBarDelegate>(
      safety_tip_status, url, web_contents, std::move(close_callback));
  infobar_service->AddInfoBar(
      SafetyTipInfoBar::CreateInfoBar(std::move(delegate)));
}

}  // namespace safety_tips

SafetyTipInfoBarDelegate::SafetyTipInfoBarDelegate(
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& url,
    WebContents* web_contents,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback)
    : safety_tip_status_(safety_tip_status),
      url_(url),
      close_callback_(std::move(close_callback)),
      web_contents_(web_contents) {}

SafetyTipInfoBarDelegate::~SafetyTipInfoBarDelegate() {
  std::move(close_callback_).Run(action_taken_);
}

base::string16 SafetyTipInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_SUMMARY);
}

int SafetyTipInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 SafetyTipInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  switch (button) {
    case BUTTON_OK:
      return l10n_util::GetStringUTF16(IDS_SAFETY_TIP_ANDROID_LEAVE_BUTTON);
    case BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(IDS_SAFETY_TIP_ANDROID_IGNORE_BUTTON);
    case BUTTON_NONE:
      NOTREACHED();
  }
  NOTREACHED();
  return base::string16();
}

bool SafetyTipInfoBarDelegate::Accept() {
  action_taken_ = SafetyTipInteraction::kLeaveSite;
  LeaveSite(web_contents_);
  return true;
}

bool SafetyTipInfoBarDelegate::Cancel() {
  auto* tab = TabAndroid::FromWebContents(web_contents_);
  if (tab) {
    action_taken_ = SafetyTipInteraction::kDismiss;
    safety_tips::ReputationService::Get(tab->GetProfile())
        ->SetUserIgnore(web_contents_, url_);
  }

  return true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
SafetyTipInfoBarDelegate::GetIdentifier() const {
  return SAFETY_TIP_INFOBAR_DELEGATE;
}

int SafetyTipInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_SAFETYTIP_SHIELD;
}

void SafetyTipInfoBarDelegate::InfoBarDismissed() {
  // Called when you click the X. Treat the same as 'ignore'.
  Cancel();
}

base::string16 SafetyTipInfoBarDelegate::GetDescriptionText() const {
  return l10n_util::GetStringUTF16(
      GetSafetyTipDescriptionId(safety_tip_status_));
}
