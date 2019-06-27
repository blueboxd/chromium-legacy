// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_attached_data.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/properties.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "chrome/browser/performance_manager/public/graph/process_node.h"

namespace performance_manager {

class FrameNodeImpl;
class ProcessNodeImpl;

// A process node follows the lifetime of a RenderProcessHost.
// It may reference zero or one processes at a time, but during its lifetime, it
// may reference more than one process. This can happen if the associated
// renderer crashes, and an associated frame is then reloaded or re-navigated.
// The state of the process node goes through:
// 1. Created, no PID.
// 2. Process started, have PID - in the case where the associated render
//    process fails to start, this state may not occur.
// 3. Process died or falied to start, have exit status.
// 4. Back to 2.
class ProcessNodeImpl
    : public PublicNodeImpl<ProcessNodeImpl, ProcessNode>,
      public TypedNodeBase<ProcessNodeImpl,
                           GraphImplObserver,
                           ProcessNode,
                           ProcessNodeObserver>,
      public resource_coordinator::mojom::ProcessCoordinationUnit {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kProcess; }

  explicit ProcessNodeImpl(GraphImpl* graph);
  ~ProcessNodeImpl() override;

  void Bind(
      resource_coordinator::mojom::ProcessCoordinationUnitRequest request);

  // resource_coordinator::mojom::ProcessCoordinationUnit implementation:
  void SetExpectedTaskQueueingDuration(base::TimeDelta duration) override;
  void SetMainThreadTaskLoadIsLow(bool main_thread_task_load_is_low) override;

  // CPU usage is expressed as the average percentage of cores occupied over the
  // last measurement interval. One core fully occupied would be 100, while two
  // cores at 5% each would be 10.
  void SetCPUUsage(double cpu_usage);
  void SetProcessExitStatus(int32_t exit_status);
  void SetProcess(base::Process process, base::Time launch_time);

  // Private implementation properties.
  void set_private_footprint_kb(uint64_t private_footprint_kb) {
    private_footprint_kb_ = private_footprint_kb;
  }
  uint64_t private_footprint_kb() const { return private_footprint_kb_; }
  void set_cumulative_cpu_usage(base::TimeDelta cumulative_cpu_usage) {
    cumulative_cpu_usage_ = cumulative_cpu_usage;
  }
  base::TimeDelta cumulative_cpu_usage() const { return cumulative_cpu_usage_; }

  const base::flat_set<FrameNodeImpl*>& frame_nodes() const;

  // If this process is associated with only one page, returns that page.
  // Otherwise, returns nullptr.
  PageNodeImpl* GetPageNodeIfExclusive() const;

  // Use process_id() in preference to process().Pid(). It's always valid to
  // access, but will return kNullProcessId when the process is not valid. It
  // will also retain the process ID for a process that has exited.
  base::ProcessId process_id() const { return process_id_; }
  const base::Process& process() const { return process_; }
  base::Time launch_time() const { return launch_time_; }
  base::Optional<int32_t> exit_status() const { return exit_status_; }

  base::TimeDelta expected_task_queueing_duration() const {
    return expected_task_queueing_duration_.value();
  }

  bool main_thread_task_load_is_low() const {
    return main_thread_task_load_is_low_.value();
  }

  double cpu_usage() const { return cpu_usage_; }

  // Add |frame_node| to this process.
  void AddFrame(FrameNodeImpl* frame_node);
  // Removes |frame_node| from the set of frames hosted by this process. Invoked
  // from the destructor of FrameNodeImpl.
  void RemoveFrame(FrameNodeImpl* frame_node);

  void OnAllFramesInProcessFrozenForTesting() { OnAllFramesInProcessFrozen(); }

 protected:
  void SetProcessImpl(base::Process process,
                      base::ProcessId process_id,
                      base::Time launch_time);

 private:
  friend class FrozenFrameAggregatorAccess;

  // ProcessNode implementation. These are private so that users of the impl use
  // the private getters rather than the public interface.
  base::ProcessId GetProcessId() const override;
  const base::Process& GetProcess() const override;
  base::Time GetLaunchTime() const override;
  base::Optional<int32_t> GetExitStatus() const override;
  void VisitFrameNodes(const FrameNodeVisitor& visitor) const override;
  base::flat_set<const FrameNode*> GetFrameNodes() const override;
  base::TimeDelta GetExpectedTaskQueueingDuration() const override;
  bool GetMainThreadTaskLoadIsLow() const override;
  double GetCpuUsage() const override;
  base::TimeDelta GetCumulativeCpuUsage() const override;
  uint64_t GetPrivateFootprintKb() const override;

  void OnAllFramesInProcessFrozen();

  void LeaveGraph() override;

  mojo::Binding<resource_coordinator::mojom::ProcessCoordinationUnit> binding_;

  base::TimeDelta cumulative_cpu_usage_;
  uint64_t private_footprint_kb_ = 0u;

  base::ProcessId process_id_ = base::kNullProcessId;
  base::Process process_;
  base::Time launch_time_;
  base::Optional<int32_t> exit_status_;

  ObservedProperty::NotifiesAlways<
      base::TimeDelta,
      &GraphImplObserver::OnExpectedTaskQueueingDurationSample,
      &ProcessNodeObserver::OnExpectedTaskQueueingDurationSample>
      expected_task_queueing_duration_;
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &GraphImplObserver::OnMainThreadTaskLoadIsLow,
      &ProcessNodeObserver::OnMainThreadTaskLoadIsLow>
      main_thread_task_load_is_low_{false};
  double cpu_usage_ = 0;

  base::flat_set<FrameNodeImpl*> frame_nodes_;

  // Inline storage for FrozenFrameAggregator user data.
  InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 8> frozen_frame_data_;

  DISALLOW_COPY_AND_ASSIGN(ProcessNodeImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
