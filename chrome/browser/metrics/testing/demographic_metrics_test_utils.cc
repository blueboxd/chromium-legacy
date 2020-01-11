// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/testing/demographic_metrics_test_utils.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/sync.pb.h"

namespace metrics {
namespace test {

void AddUserBirthYearAndGenderToSyncServer(
    base::WeakPtr<fake_server::FakeServer> fake_server,
    int birth_year,
    metrics::UserDemographicsProto::Gender gender) {
  DCHECK(fake_server);

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_priority_preference()->mutable_preference()->set_name(
      syncer::prefs::kSyncDemographics);
  specifics.mutable_priority_preference()->mutable_preference()->set_value(
      base::StringPrintf("{\"birth_year\":%d,\"gender\":%d}", birth_year,
                         static_cast<int>(gender)));
  fake_server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/syncer::prefs::kSyncDemographics,
          /*client_tag=*/specifics.preference().name(), specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));
}

int UpdateNetworkTimeAndGetMinimalEligibleBirthYear() {
  base::Time now = base::Time::Now();
  // Simulate the latency in the network to get the network time from the remote
  // server.
  constexpr base::TimeDelta kLatency = base::TimeDelta::FromMilliseconds(10);
  // Simulate the time taken to call UpdateNetworkTime() since the moment the
  // callback was created. When not testing with the fake sync server, the
  // callback is called when doing an HTTP request to the sync server.
  constexpr base::TimeDelta kCallbackDelay =
      base::TimeDelta::FromMilliseconds(10);
  // Simulate a network time that is a bit earlier than the now time.
  base::Time network_time = now - kCallbackDelay - kLatency;
  // Simulate the time in ticks at the moment the UpdateNetworkTime callback
  // function is created, which time should be at least 1 millisecond behind the
  // moment the callback is run to pass the DCHECK.
  base::TimeTicks post_time = base::TimeTicks::Now() - kCallbackDelay;
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      network_time,
      /*resolution=*/base::TimeDelta::FromMilliseconds(1), kLatency, post_time);

  constexpr int kEligibleAge =
      syncer::kUserDemographicsMinAgeInYears +
      syncer::kUserDemographicsBirthYearNoiseOffsetRange;

  base::Time::Exploded exploded_time;
  now.UTCExplode(&exploded_time);
  // Return the maximal birth year that is eligible for reporting the user's
  // birth year and gender. The -1 year is the extra buffer that Sync uses to
  // make sure that the user is at least 20 yo because the user only gives the
  // year of their birth date. For example, considering that the current date is
  // the 05 Jan 2019 and that the user was born the 05 Mar 1999, the age of the
  // user would be computed to 20 yo when using the year resolution, but the
  // user is in fact 19.
  return exploded_time.year - kEligibleAge - 1;
}

int GetNoisedBirthYear(int raw_birth_year, const Profile& profile) {
  int birth_year_offset = profile.GetPrefs()->GetInteger(
      syncer::prefs::kSyncDemographicsBirthYearOffset);
  return birth_year_offset + raw_birth_year;
}

}  // namespace test
}  // namespace metrics
