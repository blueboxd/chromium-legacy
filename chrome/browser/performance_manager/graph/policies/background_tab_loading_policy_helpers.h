// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_HELPERS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_HELPERS_H_

#include <stddef.h>

namespace performance_manager {

namespace policies {

// Helper function for BackgroundTabLoadingPolicy to compute the number of
// tabs that can load simultaneously.
size_t CalculateMaxSimultaneousTabLoads(size_t lower_bound,
                                        size_t upper_bound,
                                        size_t cores_per_load,
                                        size_t num_cores);

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_HELPERS_H_
