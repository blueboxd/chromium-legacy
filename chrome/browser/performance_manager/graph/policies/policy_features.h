// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_POLICY_FEATURES_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_POLICY_FEATURES_H_

namespace performance_manager {
namespace features {

// TODO(bgeffon): The WorkingSetTrimmer for windows feature should also be moved
// from resource manager to here.
#if defined(OS_CHROMEOS)
namespace chromeos {

// The trim on Memory Pressure feature will trim a process nodes working set
// according to the parameters below.
extern const base::Feature kTrimOnMemoryPressure;

// The trim on freeze feature will trim the working set of a process when all
// frames are frozen.
extern const base::Feature kTrimOnFreeze;

// The graph walk backoff is the _minimum_ backoff time between graph walks
// under moderate pressure in seconds. By default we will not walk more than
// once every 2 minutes.
extern const base::FeatureParam<int> kGraphWalkBackoffTimeSec;

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
extern const base::FeatureParam<int> kNodeInvisibileTimeSec;

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
extern const base::FeatureParam<int> kNodeTrimBackoffTimeSec;

struct TrimOnMemoryPressureParams {
  TrimOnMemoryPressureParams();
  TrimOnMemoryPressureParams(const TrimOnMemoryPressureParams& other);

  // GetParams will return this struct with the populated parameters below.
  static TrimOnMemoryPressureParams GetParams();

  base::TimeDelta graph_walk_backoff_time;
  base::TimeDelta node_invisible_time;
  base::TimeDelta node_trim_backoff_time;
};

}  // namespace chromeos
#endif

}  // namespace features
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_POLICY_FEATURES_H_
