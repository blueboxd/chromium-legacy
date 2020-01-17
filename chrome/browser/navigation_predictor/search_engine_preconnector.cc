// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "net/base/features.h"

namespace features {
// Feature to control preconnect to search.
const base::Feature kPreconnectToSearch{"PreconnectToSearch",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features

SearchEnginePreconnector::SearchEnginePreconnector(
    content::BrowserContext* browser_context)
    :
#if defined(OS_ANDROID)
      application_status_listener_(
          base::android::ApplicationStatusListener::New(base::BindRepeating(
              &SearchEnginePreconnector::OnApplicationStateChange,
              // It's safe to use base::Unretained here since the application
              // state listener is owned by |this|. So, no callbacks can
              // arrive after |this| has been destroyed.
              base::Unretained(this)))),
#endif  // defined(OS_ANDROID)
      browser_context_(browser_context),
      currently_in_foreground_(true) {
  DCHECK(!browser_context_->IsOffTheRecord());

#if defined(OS_ANDROID)
  auto application_state = base::android::ApplicationStatusListener::GetState();

  currently_in_foreground_ =
      application_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
      application_state ==
          base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;
#endif  // defined(OS_ANDROID)
}

SearchEnginePreconnector::~SearchEnginePreconnector() = default;

#if defined(OS_ANDROID)
void SearchEnginePreconnector::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!application_status_listener_)
    return;

  OnAppStateChanged(
      application_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
      application_state ==
          base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
}
#endif  // defined(OS_ANDROID)

void SearchEnginePreconnector::OnAppStateChangedForTesting(bool in_foreground) {
  OnAppStateChanged(in_foreground);
}

void SearchEnginePreconnector::OnAppStateChanged(bool in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (currently_in_foreground_ == in_foreground)
    return;

  currently_in_foreground_ = in_foreground;

  if (!currently_in_foreground_) {
    // Stop any future preconnects while in background.
    timer_.Stop();
    return;
  }

  StartPreconnecting(/*with_startup_delay=*/false);
}

void SearchEnginePreconnector::StartPreconnecting(bool with_startup_delay) {
  if (!currently_in_foreground_)
    return;

  if (with_startup_delay) {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(
            base::GetFieldTrialParamByFeatureAsInt(
                features::kPreconnectToSearch, "startup_delay_ms", 1000)),
        base::BindOnce(&SearchEnginePreconnector::PreconnectDSE,
                       base::Unretained(this)));
    return;
  }

  PreconnectDSE();
}

void SearchEnginePreconnector::PreconnectDSE() {
  DCHECK(!browser_context_->IsOffTheRecord());
  DCHECK(currently_in_foreground_);
  DCHECK(!timer_.IsRunning());

  if (!base::FeatureList::IsEnabled(features::kPreconnectToSearch))
    return;

  GURL preconnect_url = GetDefaultSearchEngineOriginURL();
  if (preconnect_url.scheme() != url::kHttpScheme &&
      preconnect_url.scheme() != url::kHttpsScheme) {
    return;
  }

  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));

  if (!loading_predictor)
    return;

  loading_predictor->PreconnectURLIfAllowed(
      preconnect_url, /*allow_credentials=*/true,
      net::NetworkIsolationKey(url::Origin::Create(preconnect_url),
                               url::Origin::Create(preconnect_url)));

  loading_predictor->PreconnectURLIfAllowed(
      preconnect_url, /*allow_credentials=*/false, net::NetworkIsolationKey());

  // The delay beyond the idle socket timeout that net uses when
  // re-preconnecting. If negative, no retries occur.
  constexpr base::TimeDelta kRelayDelay = base::TimeDelta::FromMilliseconds(50);

  // Set/Reset the timer to fire after the preconnect times out. Add an extra
  // delay to make sure the preconnect has expired if it wasn't used.
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          net::features::kNetUnusedIdleSocketTimeout,
          "unused_idle_socket_timeout_seconds", 60)) +
          kRelayDelay,
      base::BindOnce(&SearchEnginePreconnector::PreconnectDSE,
                     base::Unretained(this)));
}

GURL SearchEnginePreconnector::GetDefaultSearchEngineOriginURL() const {
  auto* template_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  if (!template_service)
    return GURL();
  return template_service->GetDefaultSearchProvider()
      ->GenerateSearchURL({})
      .GetOrigin();
}
