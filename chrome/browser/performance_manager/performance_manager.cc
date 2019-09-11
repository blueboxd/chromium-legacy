// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/performance_manager.h"

#include "chrome/browser/performance_manager/performance_manager_impl.h"

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

  // Passing |pm| unretained is safe as it is actually destroyed on the
  // destination sequence, and g_performance_manager.Get() would return nullptr
  // if its deletion task was already posted.
  auto* pm = PerformanceManagerImpl::GetInstance();
  // TODO(siggi): Unwrap this by binding the loose param.
  pm->task_runner_->PostTask(
      from_here, base::BindOnce(&PerformanceManagerImpl::RunCallbackWithGraph,
                                base::Unretained(pm), std::move(callback)));
}

// static
void PerformanceManager::PassToGraph(const base::Location& from_here,
                                     std::unique_ptr<GraphOwned> graph_owned) {
  DCHECK(graph_owned);

  // Passing |graph_| unretained is safe as it is actually destroyed on the
  // destination sequence, and g_performance_manager.Get() would return nullptr
  // if its deletion task was already posted.
  auto* pm = PerformanceManagerImpl::GetInstance();
  pm->task_runner_->PostTask(
      from_here,
      base::BindOnce(&GraphImpl::PassToGraph, base::Unretained(&pm->graph_),
                     std::move(graph_owned)));
}

}  // namespace performance_manager
