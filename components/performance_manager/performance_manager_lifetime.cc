// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/performance_manager_lifetime.h"

#include "base/bind.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {

namespace {

void DefaultGraphCreatedCallback(
    GraphCreatedCallback external_graph_created_callback,
    GraphImpl* graph) {
  graph->PassToGraph(std::make_unique<PageLoadTrackerDecorator>());
  std::move(external_graph_created_callback).Run(graph);
}

}  // namespace

std::unique_ptr<PerformanceManager>
CreatePerformanceManagerWithDefaultDecorators(
    GraphCreatedCallback graph_created_callback) {
  return PerformanceManagerImpl::Create(base::BindOnce(
      &DefaultGraphCreatedCallback, std::move(graph_created_callback)));
}

void DestroyPerformanceManager(std::unique_ptr<PerformanceManager> instance) {
  PerformanceManagerImpl::Destroy(std::move(instance));
}

}  // namespace performance_manager
