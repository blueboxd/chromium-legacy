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
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using safety_tips::SafetyTipInfoBarDelegate;
using safety_tips::SafetyTipType;

namespace safety_tips {

// From safety_tip_ui.h
void ShowSafetyTipDialog(content::WebContents* web_contents,
                         SafetyTipType type,
                         const GURL& url) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::make_unique<SafetyTipInfoBarDelegate>(type, url, web_contents)));
}

}  // namespace safety_tips

SafetyTipInfoBarDelegate::SafetyTipInfoBarDelegate(SafetyTipType type,
                                                   const GURL& url,
                                                   WebContents* web_contents)
    : type_(type), url_(url), web_contents_(web_contents) {}

base::string16 SafetyTipInfoBarDelegate::GetMessageText() const {
  size_t offset;
  const base::string16 safety_tip_name =
      l10n_util::GetStringUTF16(IDS_SAFETY_TIP_ANDROID_NAME);
  const base::string16 title_text = l10n_util::GetStringFUTF16(
      GetSafetyTipTitleId(type_), safety_tip_name, &offset);

  return base::JoinString(
      {title_text, l10n_util::GetStringUTF16(GetSafetyTipDescriptionId(type_))},
      base::ASCIIToUTF16("\n\n"));
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
  LeaveSite(web_contents_);
  return true;
}

bool SafetyTipInfoBarDelegate::Cancel() {
  auto* tab = TabAndroid::FromWebContents(web_contents_);
  if (tab) {
    safety_tips::ReputationService::Get(tab->GetProfile())->SetUserIgnore(url_);
  }

  return true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
SafetyTipInfoBarDelegate::GetIdentifier() const {
  return SAFETY_TIP_INFOBAR_DELEGATE;
}

int SafetyTipInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_TIP;
}

void SafetyTipInfoBarDelegate::InfoBarDismissed() {
  // Called when you click the X. Treat the same as 'ignore'.
  Cancel();
}
