// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TESTING_DEMOGRAPHIC_METRICS_TEST_UTILS_H_
#define CHROME_BROWSER_METRICS_TESTING_DEMOGRAPHIC_METRICS_TEST_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

// Helpers to support testing the user's noised birth year and gender metrics in
// browser tests.

class Profile;

namespace metrics {
namespace test {

// Parameters for the parameterized tests.
struct DemographicsTestParams {
  // Enable the feature to report the user's birth year and gender.
  bool enable_feature = false;
  // Expectation for the user's noised birth year and gender to be reported.
  // Having |enable_feature| set to true does not necessarily mean that
  // |expect_reported_demographics| will be true because other conditions might
  // stop the reporting of the user's noised birth year and gender, e.g.,
  // sync is turned off.
  bool expect_reported_demographics = false;
};

// Adds the User Demographic priority pref to the sync |fake_server|, which
// contains the test synced user's |birth_year| and |gender|.
void AddUserBirthYearAndGenderToSyncServer(
    base::WeakPtr<fake_server::FakeServer> fake_server,
    int birth_year,
    metrics::UserDemographicsProto::Gender gender);

// Updates the network time that is used to compute the test synced user's age
// and returns the minimal elibible birth year for the user to provide their
// birth year and gender.
int UpdateNetworkTimeAndGetMinimalEligibleBirthYear();

// Gets the noised birth year of the user, where the |raw_birth_year|
// corresponds to the user birth year to noise and |profile| corresponds to the
// profile of the user that has the noise pref. This function should be run
// after the Demographic Metrics Provider is run.
int GetNoisedBirthYear(int raw_birth_year, const Profile& profile);

}  // namespace test
}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TESTING_DEMOGRAPHIC_METRICS_TEST_UTILS_H_
