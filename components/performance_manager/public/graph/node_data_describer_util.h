// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_UTIL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_UTIL_H_

#include <sstream>
#include <string>

#include "base/time/time.h"
#include "base/values.h"

namespace performance_manager {

// Converts a Mojo enum to a human-readable string.
template <typename T>
std::string MojoEnumToString(T value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

// Returns a human-friendly string value computed from |time_ticks|. That string
// represents the time delta between |time_ticks| and TimeTicks::Now() in the
// following format: "x hours, y minutes".
base::Value TimeDeltaFromNowToValue(base::TimeTicks time_ticks);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_UTIL_H_
