// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"

#include <algorithm>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/stl_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

PrivacyBudgetUkmEntryFilter::PrivacyBudgetUkmEntryFilter(
    IdentifiabilityStudySettings* settings)
    : identifiability_study_settings_(settings) {}

bool PrivacyBudgetUkmEntryFilter::FilterEntry(
    ukm::mojom::UkmEntry* entry,
    base::flat_set<uint64_t>* removed_metric_hashes) const {
  const bool enabled = identifiability_study_settings_->enabled();

  // We don't yet deal with any event other than Identifiability. All other
  // types of events pass through.
  if (entry->event_hash != ukm::builders::Identifiability::kEntryNameHash)
    return true;

  // If the study is not enabled, drop all identifiability events.
  if (!enabled || entry->metrics.empty())
    return false;

  base::EraseIf(entry->metrics, [&](auto metric) {
    return !identifiability_study_settings_->ShouldRecordSurface(
        blink::IdentifiableSurface::FromMetricHash(metric.first));
  });

  // Identifiability metrics can leak information simply by being measured.
  // Hence the metrics that are filtered out aren't returning in
  // |removed_metric_hashes|.
  return !entry->metrics.empty();
}
