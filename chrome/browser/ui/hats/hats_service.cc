// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

namespace {
// Which survey we're triggering
constexpr char kHatsSurveyTrigger[] = "survey";

constexpr char kHatsSurveyProbability[] = "probability";

constexpr char kHatsSurveyEnSiteID[] = "en_site_id";

constexpr char kHatsSurveyTriggerDefault[] = "test";

constexpr double kHatsSurveyProbabilityDefault = 0;

constexpr char kHatsSurveyEnSiteIDDefault[] = "z4cctguzopq5x2ftal6vdgjrui";

constexpr char kHatsSurveyTriggerSatisfaction[] = "satisfaction";

}  // namespace

HatsService::HatsService()
    : trigger_(base::FeatureParam<std::string>(
                   &features::kHappinessTrackingSurveysForDesktop,
                   kHatsSurveyTrigger,
                   kHatsSurveyTriggerDefault)
                   .Get()),
      probability_(base::FeatureParam<double>(
                       &features::kHappinessTrackingSurveysForDesktop,
                       kHatsSurveyProbability,
                       kHatsSurveyProbabilityDefault)
                       .Get()),
      en_site_id_(base::FeatureParam<std::string>(
                      &features::kHappinessTrackingSurveysForDesktop,
                      kHatsSurveyEnSiteID,
                      kHatsSurveyEnSiteIDDefault)
                      .Get()) {}

void HatsService::LaunchSatisfactionSurvey() {
  if (ShouldShowSurvey(kHatsSurveyTriggerSatisfaction)) {
    Browser* browser = chrome::FindLastActive();
    if (browser && browser->is_type_tabbed())
      browser->window()->ShowHatsBubbleFromAppMenuButton();
  }
}

bool HatsService::ShouldShowSurvey(const std::string& trigger) const {
  bool consent_given =
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven();
  if (!consent_given)
    return false;

  if ((trigger_ == trigger || trigger_ == kHatsSurveyTriggerDefault) &&
      !launch_hats_) {
    if (base::RandDouble() < probability_) {
      // we only want to ever show hats once per profile.
      launch_hats_ = true;
      return true;
    }
    // TODO add pref checks to avoid too many surveys for a single profile
  }
  return false;
}

// static
bool HatsService::launch_hats_ = false;
