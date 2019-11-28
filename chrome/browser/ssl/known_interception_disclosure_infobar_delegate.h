// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE_H_

#include <algorithm>
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"

namespace base {
class Clock;
}  // namespace base

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class Profile;

// Singleton that tracks the known interception disclosure cooldown time.
class KnownInterceptionDisclosureCooldown {
 public:
  static KnownInterceptionDisclosureCooldown* GetInstance();

  bool IsKnownInterceptionDisclosureCooldownActive(Profile* profile);
  void ActivateKnownInterceptionDisclosureCooldown(Profile* profile);

  bool get_has_seen_known_interception() {
    return has_seen_known_interception_;
  }
  void set_has_seen_known_interception(bool has_seen) {
    has_seen_known_interception_ = has_seen;
  }

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  friend struct base::DefaultSingletonTraits<
      KnownInterceptionDisclosureCooldown>;

  KnownInterceptionDisclosureCooldown();
  ~KnownInterceptionDisclosureCooldown();

  std::unique_ptr<base::Clock> clock_;
  bool has_seen_known_interception_ = false;

#if !defined(OS_ANDROID)
  base::Time last_dismissal_time_;
#endif
};

// Shows the known interception disclosure UI if it has not been recently
// dismissed.
void MaybeShowKnownInterceptionDisclosureDialog(
    content::WebContents* web_contents,
    net::CertStatus cert_status);

class KnownInterceptionDisclosureInfoBarDelegate
    : public ConfirmInfoBarDelegate {
 public:
  explicit KnownInterceptionDisclosureInfoBarDelegate(Profile* profile);
  ~KnownInterceptionDisclosureInfoBarDelegate() override = default;

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

  // infobars::InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;
  bool ShouldExpire(const NavigationDetails& details) const override;

#if defined(OS_ANDROID)
  int GetIconId() const override;
  // This function is the equivalent of GetMessageText(), but for the portion of
  // the infobar below the 'message' title for the Android infobar.
  base::string16 GetDescriptionText() const;
#endif

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE_H_
