// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include "components/performance_manager/performance_manager_impl.h"

namespace performance_manager {

PerformanceManager::PerformanceManager() = default;
PerformanceManager::~PerformanceManager() = default;

// static
bool PerformanceManager::IsAvailable() {
  return PerformanceManagerImpl::GetInstance();
}

// static
void PerformanceManager::CallOnGraph(const base::Location& from_here,
                                     GraphCallback callback) {
  DCHECK(callback);

  // TODO(siggi): Unwrap this by binding the loose param.
  PerformanceManagerImpl::GetTaskRunner()->PostTask(
      from_here, base::BindOnce(&PerformanceManagerImpl::RunCallbackWithGraph,
                                std::move(callback)));
}

// static
void PerformanceManager::PassToGraph(const base::Location& from_here,
                                     std::unique_ptr<GraphOwned> graph_owned) {
  DCHECK(graph_owned);

  // PassToGraph() should only be called when a graph is available to take
  // ownership of |graph_owned|.
  DCHECK(IsAvailable());

  PerformanceManagerImpl::CallOnGraphImpl(
      from_here,
      base::BindOnce(
          [](std::unique_ptr<GraphOwned> graph_owned, GraphImpl* graph) {
            graph->PassToGraph(std::move(graph_owned));
          },
          std::move(graph_owned)));
}

}  // namespace performance_manager
