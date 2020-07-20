// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"

namespace performance_manager {
namespace policies {

UrgentPageDiscardingPolicy::UrgentPageDiscardingPolicy() = default;
UrgentPageDiscardingPolicy::~UrgentPageDiscardingPolicy() = default;

void UrgentPageDiscardingPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  RegisterMemoryPressureListener();
}

void UrgentPageDiscardingPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnregisterMemoryPressureListener();
  graph_ = nullptr;
}

void UrgentPageDiscardingPolicy::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (level != base::MemoryPressureListener::MemoryPressureLevel::
                   MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  // The Memory Pressure Monitor will send notifications at regular interval,
  // it's important to unregister the pressure listener to ensure that we don't
  // reply to multiple notifications at the same time.
  UnregisterMemoryPressureListener();

  PageDiscardingHelper::GetFromGraph(graph_)->UrgentlyDiscardAPage(
      features::UrgentDiscardingParams::GetParams().discard_strategy(),
      base::BindOnce(
          [](UrgentPageDiscardingPolicy* policy, bool success_unused) {
            policy->RegisterMemoryPressureListener();
          },
          base::Unretained(this)));
}

void UrgentPageDiscardingPolicy::RegisterMemoryPressureListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!memory_pressure_listener_);
  DCHECK(PageDiscardingHelper::GetFromGraph(graph_))
      << "A PageDiscardingHelper instance should be registered against the "
         "graph in order to use this policy.";

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&UrgentPageDiscardingPolicy::OnMemoryPressure,
                          base::Unretained(this)));
}

void UrgentPageDiscardingPolicy::UnregisterMemoryPressureListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  memory_pressure_listener_.reset();
}

}  // namespace policies
}  // namespace performance_manager
