// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_UTIL_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_UTIL_H_

#include <string>

#include "components/optimization_guide/proto/models.pb.h"

// Returns the string than can be used to record histograms for the optimization
// target. If adding a histogram to use the string or adding an optimization
// target, update the OptimizationGuide.OptimizationTargets histogram suffixes
// in histograms.xml.
std::string GetStringNameForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target);

// Returns false if the host is an IP address, localhosts, or an invalid
// host that is not supported by the remote optimization guide.
bool IsHostValidToFetchFromRemoteOptimizationGuide(const std::string& host);

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_UTIL_H_
