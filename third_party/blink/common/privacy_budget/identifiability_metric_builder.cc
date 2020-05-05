// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"

#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

IdentifiabilityMetricBuilder::IdentifiabilityMetricBuilder(
    base::UkmSourceId source_id)
    : ukm::internal::UkmEntryBuilderBase(
          source_id,
          ukm::builders::Identifiability::kEntryNameHash) {}

IdentifiabilityMetricBuilder::~IdentifiabilityMetricBuilder() = default;

IdentifiabilityMetricBuilder& IdentifiabilityMetricBuilder::Set(
    IdentifiableSurface surface,
    int64_t value) {
  SetMetricInternal(surface.ToUkmMetricHash(), value);
  return *this;
}

void IdentifiabilityMetricBuilder::Record(ukm::UkmRecorder* recorder) {
  // Consume the entry, but don't pass it downstream.
  (void)TakeEntry();
}

}  // namespace blink
