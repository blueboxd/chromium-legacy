// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_FLASH_DEPRECATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGINS_FLASH_DEPRECATION_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

class Profile;
class InfoBarService;

namespace base {
class Time;
}

class FlashDeprecationInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(InfoBarService* infobar_service, Profile* profile);

  // Returns true if we should display a deprecation warning for
  // |host_content_settings_map|.
  static bool ShouldDisplayFlashDeprecation(Profile* profile);

  explicit FlashDeprecationInfoBarDelegate(Profile* profile);

  // ConfirmInfobarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  void InfoBarDismissed() override;
  bool ShouldExpire(const NavigationDetails& details) const override;

 private:
  // The profile associated with this infobar.
  Profile* const profile_;

  // The time at which the banner has started to be displayed. Used to determine
  // if the banner should expire on navigation, based on how long it has been
  // visible.
  base::Time display_start_;
};

#endif  // CHROME_BROWSER_PLUGINS_FLASH_DEPRECATION_INFOBAR_DELEGATE_H_
