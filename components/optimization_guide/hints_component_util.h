// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_HINTS_COMPONENT_UTIL_H_

#include <memory>

namespace optimization_guide {

struct HintsComponentInfo;
namespace proto {
class Configuration;
}  // namespace proto

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
