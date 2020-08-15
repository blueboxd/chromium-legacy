// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_renderer_warmup_client.h"

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_process_host.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
const base::Feature kNavigationPredictorRendererWarmup{
    "NavigationPredictorRendererWarmup", base::FEATURE_DISABLED_BY_DEFAULT};
}

NavigationPredictorRendererWarmupClient::
    ~NavigationPredictorRendererWarmupClient() = default;
NavigationPredictorRendererWarmupClient::
    NavigationPredictorRendererWarmupClient(Profile* profile,
                                            const base::TickClock* clock)
    : profile_(profile),
      counterfactual_(base::GetFieldTrialParamByFeatureAsBool(
          kNavigationPredictorRendererWarmup,
          "counterfactual",
          false)),
      mem_threshold_mb_(base::GetFieldTrialParamByFeatureAsInt(
          kNavigationPredictorRendererWarmup,
          "mem_threshold_mb",
          1024)),
      cooldown_duration_(base::TimeDelta::FromMilliseconds(
          base::GetFieldTrialParamByFeatureAsInt(
              kNavigationPredictorRendererWarmup,
              "cooldown_duration_ms",
              60 * 1000))) {
  if (clock) {
    tick_clock_ = clock;
  } else {
    tick_clock_ = base::DefaultTickClock::GetInstance();
  }
}

void NavigationPredictorRendererWarmupClient::OnPredictionUpdated(
    const base::Optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  if (!prediction) {
    return;
  }

  if (prediction->prediction_source() !=
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage) {
    return;
  }

  if (!prediction->source_document_url()) {
    return;
  }

  if (!prediction->source_document_url()->is_valid()) {
    return;
  }

  if (!IsEligibleForWarmupOnCommonCriteria()) {
    return;
  }

  // TODO(robertogden): Actually use the predicted URLs.

  RecordMetricsAndMaybeDoWarmup();
}

void NavigationPredictorRendererWarmupClient::DoRendererWarmpup() {
  content::RenderProcessHost::WarmupSpareRenderProcessHost(profile_);
}

bool NavigationPredictorRendererWarmupClient::BrowserHasSpareRenderer() const {
  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->IsUnused()) {
      return true;
    }
  }
  return false;
}

bool NavigationPredictorRendererWarmupClient::
    IsEligibleForWarmupOnCommonCriteria() const {
  if (!base::FeatureList::IsEnabled(kNavigationPredictorRendererWarmup)) {
    return false;
  }

  base::TimeDelta duration_since_last_warmup =
      tick_clock_->NowTicks() - last_warmup_time_;
  if (cooldown_duration_ >= duration_since_last_warmup) {
    return false;
  }

  if (mem_threshold_mb_ >= base::SysInfo::AmountOfPhysicalMemoryMB()) {
    return false;
  }

  if (BrowserHasSpareRenderer()) {
    return false;
  }

  return true;
}

void NavigationPredictorRendererWarmupClient::RecordMetricsAndMaybeDoWarmup() {
  last_warmup_time_ = tick_clock_->NowTicks();

  if (counterfactual_) {
    return;
  }

  DoRendererWarmpup();
}
