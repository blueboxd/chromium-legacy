// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/working_set_trimmer_observer_win.h"

#include "base/logging.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"

namespace performance_manager {

WorkingSetTrimmer::WorkingSetTrimmer() = default;
WorkingSetTrimmer::~WorkingSetTrimmer() = default;

void WorkingSetTrimmer::OnPassedToGraph(Graph* graph) {
  RegisterObservers(graph);
}

void WorkingSetTrimmer::OnTakenFromGraph(Graph* graph) {
  UnregisterObservers(graph);
}

void WorkingSetTrimmer::OnAllFramesInProcessFrozen(
    const ProcessNode* process_node) {
  auto* trimmer = mechanism::WorkingSetTrimmer::GetInstance();

  if (process_node->GetProcess().IsValid() &&
      trimmer->PlatformSupportsWorkingSetTrim())
    trimmer->TrimWorkingSet(process_node);
}

void WorkingSetTrimmer::RegisterObservers(Graph* graph) {
  graph->AddProcessNodeObserver(this);
}

void WorkingSetTrimmer::UnregisterObservers(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
}

}  // namespace performance_manager
