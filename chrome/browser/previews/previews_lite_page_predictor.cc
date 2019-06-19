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

bool PreviewsLitePagePredictor::ShouldPreresolveOnPage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!previews::params::LitePageRedirectPreviewShouldPresolve())
    return false;

  if (!web_contents()->GetController().GetLastCommittedEntry())
    return false;

  if (web_contents()->GetController().GetPendingEntry())
    return false;

  if (!DataSaverIsEnabled())
    return false;

  // TODO(crbug.com/971918): Maybe preconnect to origin if this is true by
  // returning an optional URL.
  if (!previews::params::IsLitePageServerPreviewsEnabled())
    return false;

  if (!ECTIsSlow())
    return false;

  GURL url = web_contents()->GetController().GetLastCommittedEntry()->GetURL();

  if (!url.SchemeIs(url::kHttpsScheme))
    return false;

  if (PageIsBlacklisted(url))
    return false;

  if (previews::IsLitePageRedirectPreviewDomain(url))
    return false;

  if (!IsVisible())
    return false;

  return true;
}

void PreviewsLitePagePredictor::MaybeTogglePreresolveTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the timer is not null, it should be running.
  DCHECK(!timer_ || timer_->IsRunning());

  bool should_have_timer = ShouldPreresolveOnPage();
  if (should_have_timer == bool(timer_))
    return;

  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.ToggledPreresolve",
                        should_have_timer);

  if (should_have_timer) {
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

void PreviewsLitePagePredictor::Preresolve() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(timer_);

  // TODO(crbug.com/971918): Also need to preconnect to origin.
  return;

  GURL previews_url = PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
      web_contents()->GetController().GetLastCommittedEntry()->GetURL());

  predictors::LoadingPredictor* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  if (!loading_predictor || !loading_predictor->preconnect_manager())
    return;

  loading_predictor->preconnect_manager()->StartPreresolveHost(previews_url);

  // Record a local histogram for browser testing.
  base::BooleanHistogram::FactoryGet("Previews.ServerLitePage.Preresolved",
                                     base::HistogramBase::kNoFlags)
      ->Add(true);
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
