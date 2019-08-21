// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_

#include <stdint.h>
#include <string>

#include "base/optional.h"

// A representation of optimization guide information related to a navigation.
class OptimizationGuideNavigationData {
 public:
  explicit OptimizationGuideNavigationData(int64_t navigation_id);
  ~OptimizationGuideNavigationData();

  OptimizationGuideNavigationData(const OptimizationGuideNavigationData& other);

  // The navigation ID of the navigation handle that this data is associated
  // with.
  int64_t navigation_id() const { return navigation_id_; }

  // The serialized hints version for the hint that applied to the navigation.
  base::Optional<std::string> serialized_hint_version_string() const {
    return serialized_hint_version_string_;
  }
  void set_serialized_hint_version_string(
      const std::string& serialized_hint_version_string) {
    serialized_hint_version_string_ = serialized_hint_version_string;
  }

 private:
  // The navigation ID of the navigation handle that this data is associated
  // with.
  const int64_t navigation_id_;

  // The serialized hints version for the hint that applied to the navigation.
  base::Optional<std::string> serialized_hint_version_string_ = base::nullopt;

  DISALLOW_ASSIGN(OptimizationGuideNavigationData);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
