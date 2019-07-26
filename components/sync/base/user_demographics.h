// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_
#define COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_

#include "third_party/metrics_proto/user_demographics.pb.h"

namespace syncer {

// Default value for user gender when no value has been set.
constexpr int kUserDemographicsGenderDefaultValue = -1;

// Default value for user gender enum when no value has been set.
constexpr metrics::UserDemographicsProto_Gender
    kUserDemographicGenderDefaultEnumValue =
        metrics::UserDemographicsProto_Gender_Gender_MIN;

// Default value for user gender when no value has been set.
constexpr int kUserDemographicsBirthYearDefaultValue = -1;

// Default value for birth year offset when no value has been set. Set to a
// really high value that |kUserDemographicsBirthYearNoiseOffsetRange| will
// never go over.
constexpr int kUserDemographicsBirthYearNoiseOffsetDefaultValue = 100;

// Boundary of the +/- range in years within witch to randomly pick an offset to
// add to the user birth year. This adds noise to the birth year to not allow
// someone to accurately know a user's age. Possible offsets range from -2 to 2.
constexpr int kUserDemographicsBirthYearNoiseOffsetRange = 2;

// Minimal user age in years to provide demographics for.
constexpr int kUserDemographicsMinAgeInYears = 20;

// Max user age to provide demopgrahics for.
constexpr int kUserDemographicsMaxAgeInYears = 85;

// Container of user demographics.
struct UserDemographics {
  int birth_year = 0;
  metrics::UserDemographicsProto_Gender gender =
      metrics::UserDemographicsProto::Gender_MIN;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_
