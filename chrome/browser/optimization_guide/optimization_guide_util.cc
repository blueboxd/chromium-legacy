// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_util.h"

#include "base/notreached.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

std::string GetStringNameForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  switch (optimization_target) {
    case optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN:
      return "Unknown";
    case optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:
      return "PainfulPageLoad";
  }
  NOTREACHED();
  return std::string();
}

bool IsHostValidToFetchFromRemoteOptimizationGuide(const std::string& host) {
  if (net::HostStringIsLocalhost(host))
    return false;
  url::CanonHostInfo host_info;
  std::string canonicalized_host(net::CanonicalizeHost(host, &host_info));
  if (host_info.IsIPAddress() ||
      !net::IsCanonicalizedHostCompliant(canonicalized_host)) {
    return false;
  }
  return true;
}

optimization_guide::OptimizationGuideDecision
GetOptimizationGuideDecisionFromOptimizationTypeDecision(
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  switch (optimization_type_decision) {
    case optimization_guide::OptimizationTypeDecision::
        kAllowedByOptimizationFilter:
    case optimization_guide::OptimizationTypeDecision::kAllowedByHint:
      return optimization_guide::OptimizationGuideDecision::kTrue;
    case optimization_guide::OptimizationTypeDecision::kUnknown:
    case optimization_guide::OptimizationTypeDecision::
        kHadOptimizationFilterButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHadHintButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHintFetchStartedButNotAvailableInTime:
    case optimization_guide::OptimizationTypeDecision::kDeciderNotInitialized:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
    case optimization_guide::OptimizationTypeDecision::kNotAllowedByHint:
    case optimization_guide::OptimizationTypeDecision::kNoMatchingPageHint:
    case optimization_guide::OptimizationTypeDecision::kNoHintAvailable:
    case optimization_guide::OptimizationTypeDecision::
        kNotAllowedByOptimizationFilter:
      return optimization_guide::OptimizationGuideDecision::kFalse;
  }
}
