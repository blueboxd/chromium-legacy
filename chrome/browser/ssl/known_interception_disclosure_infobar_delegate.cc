// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"

#include <memory>
#include <utility>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/urls.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar.h"
#endif

namespace {

// How long to suppress the disclosure UI after showing it to the user. On
// Android, this is measured across browser sessions (which tend to be short)
// by storing the last dismissal time in a pref. On Desktop, the last dismissal
// time is stored in memory, so this is is only measured within the same
// browsing session (and thus will trigger on every browser startup).
constexpr base::TimeDelta kMessageCooldown = base::TimeDelta::FromDays(7);

}  // namespace

KnownInterceptionDisclosureCooldown*
KnownInterceptionDisclosureCooldown::GetInstance() {
  return base::Singleton<KnownInterceptionDisclosureCooldown>::get();
}

bool KnownInterceptionDisclosureCooldown::
    IsKnownInterceptionDisclosureCooldownActive(Profile* profile) {
  base::Time last_dismissal;

#if defined(OS_ANDROID)
  last_dismissal = profile->GetPrefs()->GetTime(
      prefs::kKnownInterceptionDisclosureInfobarLastShown);
#else
  last_dismissal = last_dismissal_time_;
#endif

  // More than |kMessageCooldown| days have passed.
  if (clock_->Now() - last_dismissal > kMessageCooldown)
    return false;

  return true;
}

void KnownInterceptionDisclosureCooldown::
    ActivateKnownInterceptionDisclosureCooldown(Profile* profile) {
#if defined(OS_ANDROID)
  profile->GetPrefs()->SetTime(
      prefs::kKnownInterceptionDisclosureInfobarLastShown, clock_->Now());
#else
  last_dismissal_time_ = clock_->Now();
#endif
}

void KnownInterceptionDisclosureCooldown::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

KnownInterceptionDisclosureCooldown::KnownInterceptionDisclosureCooldown()
    : clock_(std::make_unique<base::DefaultClock>()) {}
KnownInterceptionDisclosureCooldown::~KnownInterceptionDisclosureCooldown() =
    default;

void MaybeShowKnownInterceptionDisclosureDialog(
    content::WebContents* web_contents,
    net::CertStatus cert_status) {
  KnownInterceptionDisclosureCooldown* disclosure_tracker =
      KnownInterceptionDisclosureCooldown::GetInstance();
  if (!(cert_status & net::CERT_STATUS_KNOWN_INTERCEPTION_DETECTED) &&
      !disclosure_tracker->get_has_seen_known_interception()) {
    return;
  }

  disclosure_tracker->set_has_seen_known_interception(true);

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto delegate =
      std::make_unique<KnownInterceptionDisclosureInfoBarDelegate>(profile);

  infobars::InfoBar* infobar = nullptr;
  if (!KnownInterceptionDisclosureCooldown::GetInstance()
           ->IsKnownInterceptionDisclosureCooldownActive(profile)) {
#if defined(OS_ANDROID)
    infobar = infobar_service->AddInfoBar(
        KnownInterceptionDisclosureInfoBar::CreateInfoBar(std::move(delegate)));
#else
    infobar = infobar_service->AddInfoBar(
        infobar_service->CreateConfirmInfoBar(std::move(delegate)));
#endif
  }
}

KnownInterceptionDisclosureInfoBarDelegate::
    KnownInterceptionDisclosureInfoBarDelegate(Profile* profile)
    : profile_(profile) {}

base::string16 KnownInterceptionDisclosureInfoBarDelegate::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_HEADER);
}

int KnownInterceptionDisclosureInfoBarDelegate::GetButtons() const {
#if defined(OS_ANDROID)
  return BUTTON_OK;
#else
  return BUTTON_NONE;
#endif
}

base::string16 KnownInterceptionDisclosureInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
#if defined(OS_ANDROID)
  switch (button) {
    case BUTTON_OK:
      return l10n_util::GetStringUTF16(
          IDS_KNOWN_INTERCEPTION_INFOBAR_BUTTON_TEXT);
    case BUTTON_CANCEL:
      FALLTHROUGH;
    case BUTTON_NONE:
      NOTREACHED();
  }
#endif
  NOTREACHED();
  return base::string16();
}

bool KnownInterceptionDisclosureInfoBarDelegate::Accept() {
  KnownInterceptionDisclosureCooldown::GetInstance()
      ->ActivateKnownInterceptionDisclosureCooldown(profile_);
  return true;
}

base::string16 KnownInterceptionDisclosureInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}
GURL KnownInterceptionDisclosureInfoBarDelegate::GetLinkURL() const {
  return GURL("chrome://connection-monitoring-detected/");
}

infobars::InfoBarDelegate::InfoBarIdentifier
KnownInterceptionDisclosureInfoBarDelegate::GetIdentifier() const {
  return KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE;
}

void KnownInterceptionDisclosureInfoBarDelegate::InfoBarDismissed() {
  KnownInterceptionDisclosureCooldown::GetInstance()
      ->ActivateKnownInterceptionDisclosureCooldown(profile_);
  Cancel();
}

bool KnownInterceptionDisclosureInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

// Platform specific implementations.
#if defined(OS_ANDROID)
int KnownInterceptionDisclosureInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_WARNING;
}

base::string16 KnownInterceptionDisclosureInfoBarDelegate::GetDescriptionText()
    const {
  return l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_BODY1);
}

// static
void KnownInterceptionDisclosureInfoBarDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(
      prefs::kKnownInterceptionDisclosureInfobarLastShown, base::Time());
}
#endif
