// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_predictor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

PreviewsLitePagePredictor::~PreviewsLitePagePredictor() {
  if (g_browser_process->network_quality_tracker()) {
    g_browser_process->network_quality_tracker()
        ->RemoveEffectiveConnectionTypeObserver(this);
  }
}

PreviewsLitePagePredictor::PreviewsLitePagePredictor(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  drp_settings_ = DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (previews_service && previews_service->previews_ui_service() &&
      previews_service->previews_ui_service()->previews_decider_impl()) {
    opt_guide_ = previews_service->previews_ui_service()
                     ->previews_decider_impl()
                     ->previews_opt_guide();
  }

  if (g_browser_process->network_quality_tracker()) {
    g_browser_process->network_quality_tracker()
        ->AddEffectiveConnectionTypeObserver(this);
  }
}

bool PreviewsLitePagePredictor::DataSaverIsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return drp_settings_ && drp_settings_->IsDataReductionProxyEnabled();
}

bool PreviewsLitePagePredictor::ECTIsSlow() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!g_browser_process->network_quality_tracker())
    return false;

  switch (g_browser_process->network_quality_tracker()
              ->GetEffectiveConnectionType()) {
    case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
    case net::EFFECTIVE_CONNECTION_TYPE_2G:
      return true;
    case net::EFFECTIVE_CONNECTION_TYPE_3G:
    case net::EFFECTIVE_CONNECTION_TYPE_4G:
    case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
    case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
    case net::EFFECTIVE_CONNECTION_TYPE_LAST:
      return false;
  }
}

bool PreviewsLitePagePredictor::PageIsBlacklisted(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Assume the page is blacklisted if there is no optimization guide available.
  // This matches the behavior of the preview triggering itself.
  if (!opt_guide_)
    return true;

  return opt_guide_->IsBlacklisted(url,
                                   previews::PreviewsType::LITE_PAGE_REDIRECT);
}

bool PreviewsLitePagePredictor::IsVisible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_contents()->GetVisibility() == content::Visibility::VISIBLE;
}

base::Optional<GURL> PreviewsLitePagePredictor::ShouldPreresolveOnPage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!previews::params::LitePageRedirectPreviewShouldPresolve())
    return base::nullopt;

  if (!web_contents()->GetController().GetLastCommittedEntry())
    return base::nullopt;

  if (web_contents()->GetController().GetPendingEntry())
    return base::nullopt;

  if (!DataSaverIsEnabled())
    return base::nullopt;

  if (!previews::params::IsLitePageServerPreviewsEnabled())
    return base::nullopt;

  if (!ECTIsSlow())
    return base::nullopt;

  GURL url = web_contents()->GetController().GetLastCommittedEntry()->GetURL();

  if (!url.SchemeIs(url::kHttpsScheme))
    return base::nullopt;

  // Only check if the url is blacklisted if it is not a preview page.
  if (!previews::IsLitePageRedirectPreviewDomain(url) && PageIsBlacklisted(url))
    return base::nullopt;

  if (!IsVisible())
    return base::nullopt;

  // If a preview is currently being shown, preresolve the original page.
  // Otherwise, preresolve the preview.
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(url, &original_url))
    return GURL(original_url);

  return PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(url);
}

void PreviewsLitePagePredictor::MaybeTogglePreresolveTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the timer is not null, it should be running.
  DCHECK(!timer_ || timer_->IsRunning());

  url_ = ShouldPreresolveOnPage();
  if (url_.has_value() == bool(timer_))
    return;

  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.ToggledPreresolve",
                        url_.has_value());

  if (url_.has_value()) {
    timer_.reset(new base::RepeatingTimer());
    // base::Unretained is safe because the timer will stop firing once deleted,
    // and |timer_| is owned by this.
    timer_->Start(FROM_HERE,
                  previews::params::LitePageRedirectPreviewPresolveInterval(),
                  base::BindRepeating(&PreviewsLitePagePredictor::Preresolve,
                                      base::Unretained(this)));
    Preresolve();
  } else {
    // Resetting the unique_ptr will delete the timer itself, causing it to stop
    // calling its callback.
    timer_.reset();
  }
}

void PreviewsLitePagePredictor::Preresolve() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(timer_);

  UMA_HISTOGRAM_BOOLEAN(
      "Previews.ServerLitePage.PreresolvedToPreviewServer",
      previews::IsLitePageRedirectPreviewDomain(url_.value()));

  predictors::LoadingPredictor* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  if (!loading_predictor || !loading_predictor->preconnect_manager())
    return;

  loading_predictor->preconnect_manager()->StartPreresolveHost(url_.value());
}

void PreviewsLitePagePredictor::DidStartNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!handle->IsInMainFrame())
    return;
  MaybeTogglePreresolveTimer();
}

void PreviewsLitePagePredictor::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!handle->IsInMainFrame())
    return;
  MaybeTogglePreresolveTimer();
}

void PreviewsLitePagePredictor::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeTogglePreresolveTimer();
}

void PreviewsLitePagePredictor::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType ect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeTogglePreresolveTimer();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewsLitePagePredictor)
