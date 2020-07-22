// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_pmf_discard_policy.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {
namespace policies {

namespace {

// The factor that will be applied to the total amount of RAM to establish the
// PMF limit.
static constexpr base::FeatureParam<double> kRAMRatioPMFLimitFactor{
    &performance_manager::features::kHighPMFDiscardPolicy,
    "RAMRatioPMFLimitFactor", 1.5};

// The discard strategy to use.
static constexpr base::FeatureParam<int> kDiscardStrategy{
    &performance_manager::features::kHighPMFDiscardPolicy, "DiscardStrategy",
    static_cast<int>(features::DiscardStrategy::LRU)};

}  // namespace

HighPMFDiscardPolicy::HighPMFDiscardPolicy() = default;
HighPMFDiscardPolicy::~HighPMFDiscardPolicy() = default;

void HighPMFDiscardPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddSystemNodeObserver(this);
  graph_ = graph;

  base::SystemMemoryInfoKB mem_info = {};
  if (base::GetSystemMemoryInfo(&mem_info))
    pmf_limit_kb_ = mem_info.total * kRAMRatioPMFLimitFactor.Get();

  DCHECK(PageDiscardingHelper::GetFromGraph(graph_))
      << "A PageDiscardingHelper instance should be registered against the "
         "graph in order to use this policy.";
}

void HighPMFDiscardPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveSystemNodeObserver(this);
  graph_ = nullptr;
  pmf_limit_kb_ = kInvalidPMFLimitValue;
}

void HighPMFDiscardPolicy::OnProcessMemoryMetricsAvailable(
    const SystemNode* unused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pmf_limit_kb_ == kInvalidPMFLimitValue)
    return;

  if (discard_attempt_in_progress_)
    return;

  int total_pmf_kb = 0;
  bool should_discard = false;
  auto process_nodes = graph_->GetAllProcessNodes();
  for (const auto* node : process_nodes) {
    total_pmf_kb += node->GetPrivateFootprintKb();
    if (total_pmf_kb >= pmf_limit_kb_) {
      should_discard = true;
      break;
    }
  }

  if (should_discard) {
    discard_attempt_in_progress_ = true;
    PageDiscardingHelper::GetFromGraph(graph_)->UrgentlyDiscardAPage(
        static_cast<features::DiscardStrategy>(kDiscardStrategy.Get()),
        base::BindOnce(&HighPMFDiscardPolicy::PostDiscardAttemptCallback,
                       base::Unretained(this)));
  }
}

void HighPMFDiscardPolicy::PostDiscardAttemptCallback(bool success) {
  DCHECK(discard_attempt_in_progress_);
  discard_attempt_in_progress_ = false;
}

}  // namespace policies
}  // namespace performance_manager
