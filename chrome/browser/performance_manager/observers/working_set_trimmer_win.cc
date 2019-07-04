// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/working_set_trimmer_win.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>

#include "base/logging.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

namespace {

// Empties the working set of a process with id |process_id| and creation time
// |process_creation_time|. The creation time is verified to ensure that we
// don't empty the working set of the wrong process if the target process exits
// and its id is reused.
void EmptyWorkingSet(const base::Process& process,
                     base::Time process_creation_time) {
  // Open a new handle to the process with the specific access needed.
  base::Process process_copy = base::Process::OpenWithAccess(
      process.Pid(), PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_QUOTA);
  if (!process_copy.IsValid()) {
    DPLOG(ERROR) << "Working set not emptied because process handle could not "
                    "be obtained.";
    return;
  }

#if DCHECK_IS_ON()
  BOOL empty_working_set_success =
#endif
      ::EmptyWorkingSet(process.Handle());
  DPLOG_IF(ERROR, !empty_working_set_success)
      << "Working set not emptied because EmptyWorkingSet() failed";
}

}  // namespace

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
  if (process_node->GetProcess().IsValid())
    EmptyWorkingSet(process_node->GetProcess(), process_node->GetLaunchTime());
}

void WorkingSetTrimmer::RegisterObservers(Graph* graph) {
  graph->AddProcessNodeObserver(this);
}

void WorkingSetTrimmer::UnregisterObservers(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
}

}  // namespace performance_manager
