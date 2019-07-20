// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_
#define COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_

#include "third_party/metrics_proto/user_demographics.pb.h"

namespace syncer {

// Container of user demographics.
struct UserDemographics {
  int birth_year = 0;
  metrics::UserDemographicsProto_Gender gender =
      metrics::UserDemographicsProto::Gender_MIN;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_
