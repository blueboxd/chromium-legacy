// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographic_metrics_provider.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace metrics {

namespace {

// TODO(crbug/979371): Implement this.
// Gets user's birth year from Profile prefs that are provided by |service|.
// Returns false if could not retrieve birth year.
bool GetUserBirthYear(const PrefService& service, int* birth_year) {
  *birth_year = 1990;
  return true;
}

// TODO(crbug/979371): Implement this.
// Gets user's gender from Profile prefs that are provided by |service|. Returns
// false if could not retrieve birth year.
bool GetUserGender(const PrefService& service,
                   UserDemographicsProto::Gender* gender) {
  *gender = UserDemographicsProto::GENDER_UNKNOWN_OR_OTHER;
  return true;
}

}  // namespace

DemographicMetricsProvider::DemographicMetricsProvider(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  // TODO(crbug/979371): Remove this log. Only there to avoid compilation error.
  DVLOG(1) << "Using pref service instance at: " << pref_service_;
}

void DemographicMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  // Skip if feature disabled.
  if (!base::FeatureList::IsEnabled(kDemographicMetricsReporting))
    return;

  int birth_year;
  if (GetUserBirthYear(*pref_service_, &birth_year))
    uma_proto->mutable_user_demographics()->set_birth_year(birth_year);
  UserDemographicsProto::Gender user_gender;
  if (GetUserGender(*pref_service_, &user_gender))
    uma_proto->mutable_user_demographics()->set_gender(user_gender);
}

// static
const base::Feature DemographicMetricsProvider::kDemographicMetricsReporting = {
    "DemographicMetricsReporting", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace metrics
