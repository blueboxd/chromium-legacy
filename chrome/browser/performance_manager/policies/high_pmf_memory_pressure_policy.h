// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_PMF_MEMORY_PRESSURE_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_PMF_MEMORY_PRESSURE_POLICY_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

class Graph;

namespace mechanism {
class HighPMFMemoryPressureSignals;
}

namespace policies {

// The HighPMFMemoryPressurePolicy will emit critical memory pressure signal
// when Chrome's total PMF exceeds a given threshold.
class HighPMFMemoryPressurePolicy : public GraphOwned,
                                    public SystemNode::ObserverDefaultImpl {
 public:
  HighPMFMemoryPressurePolicy();
  ~HighPMFMemoryPressurePolicy() override;
  HighPMFMemoryPressurePolicy(const HighPMFMemoryPressurePolicy& other) =
      delete;
  HighPMFMemoryPressurePolicy& operator=(const HighPMFMemoryPressurePolicy&) =
      delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // SystemNode::ObserverDefaultImpl:
  void OnProcessMemoryMetricsAvailable(const SystemNode* system_node) override;

  void set_pmf_limit_for_testing(int pmf_limit_kb) {
    pmf_limit_kb_ = pmf_limit_kb;
  }

 private:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  const int kInvalidPMFLimitValue = 0;

  int pmf_limit_kb_ = kInvalidPMFLimitValue;
  std::unique_ptr<mechanism::HighPMFMemoryPressureSignals> mechanism_;
  Graph* graph_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_PMF_MEMORY_PRESSURE_POLICY_H_
