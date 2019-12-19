// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

namespace {
// The name of the icon image for the save passwords banner.
NSString* const kIconImageName = @"infobar_passwords_icon";
}

OVERLAY_USER_DATA_SETUP_IMPL(SavePasswordInfobarBannerOverlayRequestConfig);

SavePasswordInfobarBannerOverlayRequestConfig::
    SavePasswordInfobarBannerOverlayRequestConfig(InfoBar* infobar)
    : infobar_(infobar),
      save_password_delegate_(
          IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
              infobar->delegate())) {
  DCHECK(infobar_);
  DCHECK(save_password_delegate_);
}

SavePasswordInfobarBannerOverlayRequestConfig::
    ~SavePasswordInfobarBannerOverlayRequestConfig() = default;

void SavePasswordInfobarBannerOverlayRequestConfig::CreateAuxilliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(user_data, infobar_);
  InfobarOverlayRequestConfig* infobar_config =
      InfobarOverlayRequestConfig::FromUserData(user_data);
  // Use the save passwords delegate to configure the request for its banner UI.
  NSString* title_text =
      base::SysUTF16ToNSString(save_password_delegate_->GetMessageText());
  NSString* username = save_password_delegate_->GetUserNameText();
  NSString* password = save_password_delegate_->GetPasswordText();
  password = [@"" stringByPaddingToLength:[password length]
                               withString:@"•"
                          startingAtIndex:0];
  NSString* subtitle_text =
      [NSString stringWithFormat:@"%@ %@", username, password];
  NSString* button_text =
      base::SysUTF16ToNSString(save_password_delegate_->GetButtonLabel(
          ConfirmInfoBarDelegate::BUTTON_OK));
  NSString* hidden_password_text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL);
  NSString* banner_accessibility_label =
      [NSString stringWithFormat:@"%@,%@, %@", title_text, username,
                                 hidden_password_text];
  bool presents_modal = infobar_config->has_badge();
  InfobarBannerOverlayRequestConfig::CreateForUserData(
      user_data, banner_accessibility_label, button_text, kIconImageName,
      presents_modal, title_text, subtitle_text);
}
