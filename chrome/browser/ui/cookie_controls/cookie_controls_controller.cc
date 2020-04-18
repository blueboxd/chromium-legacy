
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"

#include <memory>
#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

using base::UserMetricsAction;

CookieControlsController::CookieControlsController(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  if (profile->IsOffTheRecord()) {
    regular_cookie_settings_ =
        CookieSettingsFactory::GetForProfile(profile->GetOriginalProfile());
  }

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(&CookieControlsController::OnPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::BindRepeating(&CookieControlsController::OnPrefChanged,
                          base::Unretained(this)));
}

CookieControlsController::~CookieControlsController() = default;

void CookieControlsController::OnUiClosing() {
  auto* web_contents = GetWebContents();
  if (should_reload_ && web_contents && !web_contents->IsBeingDestroyed())
    web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  should_reload_ = false;
}

void CookieControlsController::Update(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!tab_observer_ || GetWebContents() != web_contents) {
    DCHECK(content_settings::TabSpecificContentSettings::FromWebContents(
        web_contents));
    tab_observer_ = std::make_unique<TabObserver>(
        this, content_settings::TabSpecificContentSettings::FromWebContents(
                  web_contents));
  }
  auto status = GetStatus(web_contents);
  int blocked_count = GetBlockedCookieCount();
  for (auto& observer : observers_)
    observer.OnStatusChanged(status.first, status.second, blocked_count);
}

std::pair<CookieControlsStatus, CookieControlsEnforcement>
CookieControlsController::GetStatus(content::WebContents* web_contents) {
  if (!cookie_settings_->IsCookieControlsEnabled()) {
    return {CookieControlsStatus::kDisabled,
            CookieControlsEnforcement::kNoEnforcement};
  }
  const GURL& url = web_contents->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(extensions::kExtensionScheme)) {
    return {CookieControlsStatus::kDisabled,
            CookieControlsEnforcement::kNoEnforcement};
  }

  content_settings::SettingSource source;
  bool is_allowed = cookie_settings_->IsThirdPartyAccessAllowed(
      web_contents->GetURL(), &source);

  CookieControlsStatus status = is_allowed
                                    ? CookieControlsStatus::kDisabledForSite
                                    : CookieControlsStatus::kEnabled;
  CookieControlsEnforcement enforcement;
  if (source == content_settings::SETTING_SOURCE_POLICY) {
    enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  } else if (is_allowed && regular_cookie_settings_ &&
             regular_cookie_settings_->ShouldBlockThirdPartyCookies() &&
             regular_cookie_settings_->IsThirdPartyAccessAllowed(
                 web_contents->GetURL(), nullptr /* source */)) {
    // TODO(crbug.com/1015767): Rules from regular mode can't be temporarily
    // overridden in incognito.
    enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  } else {
    enforcement = CookieControlsEnforcement::kNoEnforcement;
  }
  return {status, enforcement};
}

void CookieControlsController::OnCookieBlockingEnabledForSite(
    bool block_third_party_cookies) {
  if (block_third_party_cookies) {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOn"));
    should_reload_ = false;
    cookie_settings_->ResetThirdPartyCookieSetting(GetWebContents()->GetURL());
  } else {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOff"));
    should_reload_ = true;
    cookie_settings_->SetThirdPartyCookieSetting(
        GetWebContents()->GetURL(), ContentSetting::CONTENT_SETTING_ALLOW);
  }
  Update(GetWebContents());
}

int CookieControlsController::GetBlockedCookieCount() {
  const browsing_data::LocalSharedObjectsContainer& blocked_objects =
      tab_observer_->tab_specific_content_settings()
          ->blocked_local_shared_objects();
  return blocked_objects.GetObjectCount();
}

void CookieControlsController::PresentBlockedCookieCounter() {
  int blocked_cookies = GetBlockedCookieCount();
  for (auto& observer : observers_)
    observer.OnBlockedCookiesCountChanged(blocked_cookies);
}

void CookieControlsController::OnPrefChanged() {
  if (GetWebContents())
    Update(GetWebContents());
}

content::WebContents* CookieControlsController::GetWebContents() {
  if (!tab_observer_ || !tab_observer_->tab_specific_content_settings())
    return nullptr;
  return tab_observer_->tab_specific_content_settings()->web_contents();
}

void CookieControlsController::AddObserver(CookieControlsView* obs) {
  observers_.AddObserver(obs);
}

void CookieControlsController::RemoveObserver(CookieControlsView* obs) {
  observers_.RemoveObserver(obs);
}

CookieControlsController::TabObserver::TabObserver(
    CookieControlsController* cookie_controls,
    content_settings::TabSpecificContentSettings* tab_specific_content_settings)
    : content_settings::TabSpecificContentSettings::SiteDataObserver(
          tab_specific_content_settings),
      cookie_controls_(cookie_controls) {}

void CookieControlsController::TabObserver::OnSiteDataAccessed() {
  cookie_controls_->PresentBlockedCookieCounter();
}
