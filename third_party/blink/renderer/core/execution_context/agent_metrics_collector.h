// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_METRICS_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_METRICS_COLLECTOR_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/agents/agent_metrics.mojom-blink.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace base {
class TickClock;
}

namespace blink {

class Agent;
class LocalDOMWindow;
class TimerBase;

// This class tracks agent-related metrics for reporting in TRACE and UMA
// metrics. It listens for windows being attached/detached to an execution
// context and tracks which agent these windows are associated with.
//
// We report metrics periodically to track how long we spent in any given state.
// For example, suppose that for 10 seconds a page had just one agent, then an
// ad frame loads causing a windows to load with a second agent. After 5
// seconds, the user closes the browser. In this case, we report:
//
// Histogram
// 1 ----------O        10
// 2 -----O             5
//
// We therefore keep track of how much time has elapsed since the previous
// report. Metrics are reported whenever a windows is added or removed, as
// well as at a regular interval.
//
// This class is based on the metrics tracked in:
// chrome/browser/performance_manager/observers/isolation_context_metrics.cc
// It should eventually be migrated to that place.
class AgentMetricsCollector final
    : public GarbageCollected<AgentMetricsCollector> {
 public:
  AgentMetricsCollector();
  ~AgentMetricsCollector();

  void DidAttachWindow(const LocalDOMWindow&);
  void DidDetachWindow(const LocalDOMWindow&);

  void ReportMetrics();

  void SetTickClockForTesting(const base::TickClock* clock) { clock_ = clock; }

  void Trace(Visitor*) const;

 private:
  void AddTimeToTotalAgents(int time_delta_to_add);
  void ReportToBrowser();

  void ReportingTimerFired(TimerBase*);

  blink::mojom::blink::AgentMetricsCollectorHost*
  GetAgentMetricsCollectorHost();

  std::unique_ptr<TaskRunnerTimer<AgentMetricsCollector>> reporting_timer_;
  base::TimeTicks time_last_reported_;

  // Keep a map from each agent to all the windows associated with that
  // agent. When the last window from the set is removed, we delete the key
  // from the map.
  using WindowSet = HeapHashSet<WeakMember<const LocalDOMWindow>>;
  using AgentToWindowsMap = HeapHashMap<WeakMember<Agent>, Member<WindowSet>>;
  AgentToWindowsMap agent_to_windows_map_;

  const base::TickClock* clock_;

  // AgentMetricsCollector is not tied to ExecutionContext
  HeapMojoRemote<blink::mojom::blink::AgentMetricsCollectorHost,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      agent_metrics_collector_host_{nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_METRICS_COLLECTOR_H_
