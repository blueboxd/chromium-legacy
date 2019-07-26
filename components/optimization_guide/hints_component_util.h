// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_

#include <memory>

#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

struct HintsComponentInfo;

// The local histogram used to record that the component hints are stored in
// the cache and are ready for use.
extern const char kComponentHintsUpdatedResultHistogramString[];

// Enumerates the possible outcomes of processing the hints component.
//
// Used in UMA histograms, so the order of enumerators should not be changed.
// Keep in sync with OptimizationGuideProcessHintsResult in
// tools/metrics/histograms/enums.xml.
enum class ProcessHintsComponentResult {
  kSuccess,
  kFailedInvalidParameters,
  kFailedReadingFile,
  kFailedInvalidConfiguration,
  kFailedFinishProcessing,
  kSkippedProcessingHints,
  kProcessedNoHints,

  // Insert new values before this line.
  kMaxValue = kProcessedNoHints,
};

// Records the ProcessHintsComponentResult to UMA.
void RecordProcessHintsComponentResult(ProcessHintsComponentResult result);

// Enumerates status event of processing optimization filters (such as the
// lite page redirect blacklist). Used in UMA histograms, so the order of
// enumerators should not be changed.
//
// Keep in sync with OptimizationGuideOptimizationFilterStatus in
// tools/metrics/histograms/enums.xml.
enum class OptimizationFilterStatus {
  kFoundServerBlacklistConfig,
  kCreatedServerBlacklist,
  kFailedServerBlacklistBadConfig,
  kFailedServerBlacklistTooBig,
  kFailedServerBlacklistDuplicateConfig,

  // Insert new values before this line.
  kMaxValue = kFailedServerBlacklistDuplicateConfig,
};

// Records the OptimizationFilterStatus to UMA.
void RecordOptimizationFilterStatus(proto::OptimizationType optimization_type,
                                    OptimizationFilterStatus status);

// Processes the specified hints component.
//
// If successful, returns the component's Configuration protobuf. Otherwise,
// returns a nullptr.
//
// If |out_result| provided, it will be populated with the result up to that
// point.
std::unique_ptr<proto::Configuration> ProcessHintsComponent(
    const HintsComponentInfo& info,
    ProcessHintsComponentResult* out_result);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_
