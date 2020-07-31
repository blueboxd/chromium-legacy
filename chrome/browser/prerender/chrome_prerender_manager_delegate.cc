// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/chrome_prerender_manager_delegate.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/prerender/prerender_manager_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
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

}  // namespace prerender
