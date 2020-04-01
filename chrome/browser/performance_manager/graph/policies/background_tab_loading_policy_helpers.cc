// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy_helpers.h"
#include "base/logging.h"

namespace performance_manager {

namespace policies {

size_t CalculateMaxSimultaneousTabLoads(size_t lower_bound,
                                        size_t upper_bound,
                                        size_t cores_per_load,
                                        size_t num_cores) {
  DCHECK(upper_bound == 0 || lower_bound <= upper_bound);
  DCHECK(num_cores > 0);

  size_t loads = 0;

  // Setting |cores_per_load| == 0 means that no per-core limit is applied.
  if (cores_per_load == 0) {
    loads = std::numeric_limits<size_t>::max();
  } else {
    loads = num_cores / cores_per_load;
  }

  // If |upper_bound| isn't zero then apply the maximum that it implies.
  if (upper_bound != 0)
    loads = std::min(loads, upper_bound);

  loads = std::max(loads, lower_bound);

  return loads;
}

}  // namespace policies

}  // namespace performance_manager
