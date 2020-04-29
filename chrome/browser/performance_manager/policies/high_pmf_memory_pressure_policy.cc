// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_pmf_memory_pressure_policy.h"

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/performance_manager/mechanisms/high_pmf_memory_pressure_signals.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {
namespace policies {

namespace {

// The factor that will be applied to the total amount of RAM to establish the
// PMF limit.
static constexpr base::FeatureParam<double> kRAMRatioPMFLimitFactor{
    &performance_manager::features::kHighPMFMemoryPressureSignals,
    "RAMRatioPMFLimitFactor", 1.5};

}  // namespace

HighPMFMemoryPressurePolicy::HighPMFMemoryPressurePolicy() = default;
HighPMFMemoryPressurePolicy::~HighPMFMemoryPressurePolicy() = default;

void HighPMFMemoryPressurePolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddSystemNodeObserver(this);
  graph_ = graph;

  base::SystemMemoryInfoKB mem_info = {};
  if (base::GetSystemMemoryInfo(&mem_info))
    pmf_limit_kb_ = mem_info.total * kRAMRatioPMFLimitFactor.Get();

  mechanism_ = std::make_unique<mechanism::HighPMFMemoryPressureSignals>();
}

void HighPMFMemoryPressurePolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveSystemNodeObserver(this);
  graph_ = nullptr;
  pmf_limit_kb_ = kInvalidPMFLimitValue;
  mechanism_.reset();
}

void HighPMFMemoryPressurePolicy::OnProcessMemoryMetricsAvailable(
    const SystemNode* unused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pmf_limit_kb_ == kInvalidPMFLimitValue)
    return;

  int total_pmf_kb = 0;
  MemoryPressureLevel pressure_level =
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE;
  auto process_nodes = graph_->GetAllProcessNodes();
  for (const auto* node : process_nodes) {
    total_pmf_kb += node->GetPrivateFootprintKb();
    if (total_pmf_kb >= pmf_limit_kb_) {
      pressure_level = MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL;
      break;
    }
  }

  mechanism_->SetPressureLevel(pressure_level);
}

}  // namespace policies
}  // namespace performance_manager
