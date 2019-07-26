// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_component_util.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_processing_util.h"

namespace optimization_guide {

namespace {

const char kProcessHintsComponentResultHistogramString[] =
    "OptimizationGuide.ProcessHintsResult";

void MaybePopulateProcessHintsComponentResult(
    ProcessHintsComponentResult result,
    ProcessHintsComponentResult* out_result) {
  if (out_result)
    *out_result = result;
}

}  // namespace

const char kComponentHintsUpdatedResultHistogramString[] =
    "OptimizationGuide.UpdateComponentHints.Result";

void RecordProcessHintsComponentResult(ProcessHintsComponentResult result) {
  UMA_HISTOGRAM_ENUMERATION(kProcessHintsComponentResultHistogramString,
                            result);
}

void RecordOptimizationFilterStatus(proto::OptimizationType optimization_type,
                                    OptimizationFilterStatus status) {
  std::string histogram_name = base::StringPrintf(
      "OptimizationGuide.OptimizationFilterStatus.%s",
      GetStringNameForOptimizationType(optimization_type).c_str());
  UMA_HISTOGRAM_ENUMERATION(histogram_name, status);
}

std::unique_ptr<proto::Configuration> ProcessHintsComponent(
    const HintsComponentInfo& component_info,
    ProcessHintsComponentResult* out_result) {
  if (!component_info.version.IsValid() || component_info.path.empty()) {
    MaybePopulateProcessHintsComponentResult(
        ProcessHintsComponentResult::kFailedInvalidParameters, out_result);
    return nullptr;
  }

  std::string binary_pb;
  if (!base::ReadFileToString(component_info.path, &binary_pb)) {
    MaybePopulateProcessHintsComponentResult(
        ProcessHintsComponentResult::kFailedReadingFile, out_result);
    return nullptr;
  }

  std::unique_ptr<proto::Configuration> proto_configuration =
      std::make_unique<proto::Configuration>();
  if (!proto_configuration->ParseFromString(binary_pb)) {
    MaybePopulateProcessHintsComponentResult(
        ProcessHintsComponentResult::kFailedInvalidConfiguration, out_result);
    return nullptr;
  }

  MaybePopulateProcessHintsComponentResult(
      ProcessHintsComponentResult::kSuccess, out_result);
  return proto_configuration;
}

}  // namespace optimization_guide
