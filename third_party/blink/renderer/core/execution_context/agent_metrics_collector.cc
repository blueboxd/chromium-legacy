// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent_metrics_collector.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {

const char kAgentsPerRendererByTimeHistogram[] =
    "PerformanceManager.AgentsPerRendererByTime";

base::TimeDelta kReportingInterval = base::TimeDelta::FromMinutes(5);

}  // namespace

AgentMetricsCollector::AgentMetricsCollector()
    : reporting_timer_(std::make_unique<TaskRunnerTimer<AgentMetricsCollector>>(
          // Some tests might not have a MainThreadScheduler.
          scheduler::WebThreadScheduler::MainThreadScheduler()
              ? scheduler::WebThreadScheduler::MainThreadScheduler()
                    ->DefaultTaskRunner()
              : nullptr,
          this,
          &AgentMetricsCollector::ReportingTimerFired)),
      clock_(base::DefaultTickClock::GetInstance()) {
  // From now until we call CreatedNewAgent will be reported as having 0
  // agents.
  time_last_reported_ = clock_->NowTicks();
}

AgentMetricsCollector::~AgentMetricsCollector() {
  // Note: This won't be called during a fast-shutdown (i.e. tab closed). We
  // manually call it from Page::WillBeDestroyed().
  ReportMetrics();
}

void AgentMetricsCollector::DidAttachDocument(const Document& doc) {
  ReportMetrics();

  AgentToDocumentsMap::AddResult result =
      agent_to_documents_map_.insert(doc.GetAgent(), nullptr);
  if (result.is_new_entry)
    result.stored_value->value = MakeGarbageCollected<DocumentSet>();

  result.stored_value->value->insert(&doc);
}

void AgentMetricsCollector::DidDetachDocument(const Document& doc) {
  ReportMetrics();

  auto agent_itr = agent_to_documents_map_.find(doc.GetAgent());
  DCHECK(agent_itr != agent_to_documents_map_.end());

  DocumentSet& documents = *agent_itr->value.Get();
  auto document_itr = documents.find(&doc);
  DCHECK(document_itr != documents.end());

  documents.erase(document_itr);

  if (documents.IsEmpty())
    agent_to_documents_map_.erase(agent_itr);
}

void AgentMetricsCollector::ReportMetrics() {
  DCHECK(!time_last_reported_.is_null());

  // Don't run the timer in tests. Doing so causes tests that RunUntilIdle to
  // never exit.
  if (!reporting_timer_->IsActive() && !WebTestSupport::IsRunningWebTest()) {
    reporting_timer_->StartRepeating(kReportingInterval, FROM_HERE);
  }

  // This computation and reporting is based on the one in
  // chrome/browser/performance_manager/observers/isolation_context_metrics.cc.
  base::TimeTicks now = clock_->NowTicks();

  base::TimeDelta elapsed = now - time_last_reported_;
  time_last_reported_ = now;

  // Account for edge cases like hibernate/sleep. See
  // GetSecondsSinceLastReportAndUpdate in isolation_context_metrics.cc
  if (elapsed >= 2 * kReportingInterval)
    elapsed = base::TimeDelta();

  int to_add = static_cast<int>(std::round(elapsed.InSecondsF()));

  // Time can be negative in tests when we replace the clock_.
  if (to_add <= 0)
    return;

  AddTimeToTotalAgents(to_add);
}

void AgentMetricsCollector::AddTimeToTotalAgents(int time_delta_to_add) {
  DEFINE_STATIC_LOCAL(LinearHistogram, agents_per_renderer_histogram,
                      (kAgentsPerRendererByTimeHistogram, 1, 100, 101));
  agents_per_renderer_histogram.CountMany(agent_to_documents_map_.size(),
                                          time_delta_to_add);
}

void AgentMetricsCollector::ReportingTimerFired(TimerBase*) {
  ReportMetrics();
}

void AgentMetricsCollector::Trace(blink::Visitor* visitor) {
  visitor->Trace(agent_to_documents_map_);
}

}  // namespace blink
