// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/chrome_prerender_manager_delegate.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/prerender/chrome_prerender_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prerender/browser/prerender_manager_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace prerender {

ChromePrerenderManagerDelegate::ChromePrerenderManagerDelegate(Profile* profile)
    : profile_(profile) {}

scoped_refptr<content_settings::CookieSettings>
ChromePrerenderManagerDelegate::GetCookieSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return CookieSettingsFactory::GetForProfile(profile_);
}

void ChromePrerenderManagerDelegate::MaybePreconnect(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kPrerenderFallbackToPreconnect)) {
    return;
  }

  if (GetCookieSettings()->ShouldBlockThirdPartyCookies()) {
    return;
  }

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile_);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(
        url, predictors::HintOrigin::OMNIBOX_PRERENDER_FALLBACK, true);
  }
}

std::unique_ptr<PrerenderContentsDelegate>
ChromePrerenderManagerDelegate::GetPrerenderContentsDelegate() {
  return std::make_unique<ChromePrerenderContentsDelegate>();
}

bool ChromePrerenderManagerDelegate::IsPredictionEnabled(Origin origin) {
  return GetPredictionStatusForOrigin(origin) ==
         chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

bool ChromePrerenderManagerDelegate::IsPredictionDisabledDueToNetwork(
    Origin origin) {
  return GetPredictionStatusForOrigin(origin) ==
         chrome_browser_net::NetworkPredictionStatus::DISABLED_DUE_TO_NETWORK;
}

bool ChromePrerenderManagerDelegate::IsPredictionEnabled() {
  return GetPredictionStatus() ==
         chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

std::string ChromePrerenderManagerDelegate::GetReasonForDisablingPrediction() {
  std::string disabled_note;
  if (GetPredictionStatus() ==
      chrome_browser_net::NetworkPredictionStatus::DISABLED_ALWAYS)
    disabled_note = "Disabled by user setting";
  if (GetPredictionStatus() ==
      chrome_browser_net::NetworkPredictionStatus::DISABLED_DUE_TO_NETWORK)
    disabled_note = "Disabled on cellular connection by default";
  return disabled_note;
}

chrome_browser_net::NetworkPredictionStatus
ChromePrerenderManagerDelegate::GetPredictionStatus() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return chrome_browser_net::CanPrefetchAndPrerenderUI(profile_->GetPrefs());
}

chrome_browser_net::NetworkPredictionStatus
ChromePrerenderManagerDelegate::GetPredictionStatusForOrigin(
    Origin origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // <link rel=prerender> origins ignore the network state and the privacy
  // settings. Web developers should be able prefetch with all possible privacy
  // settings and with all possible network types. This would avoid web devs
  // coming up with creative ways to prefetch in cases they are not allowed to
  // do so.
  if (origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN ||
      origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN) {
    return chrome_browser_net::NetworkPredictionStatus::ENABLED;
  }

  // Prerendering forced for cellular networks still prevents navigation with
  // the DISABLED_ALWAYS selected via privacy settings.
  chrome_browser_net::NetworkPredictionStatus prediction_status =
      chrome_browser_net::CanPrefetchAndPrerenderUI(profile_->GetPrefs());
  if (origin == ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER &&
      prediction_status == chrome_browser_net::NetworkPredictionStatus::
                               DISABLED_DUE_TO_NETWORK) {
    return chrome_browser_net::NetworkPredictionStatus::ENABLED;
  }
  return prediction_status;
}

}  // namespace prerender
