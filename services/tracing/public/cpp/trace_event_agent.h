// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/base_agent.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace tracing {

// Agent used to interface with the legacy tracing system.
// When Perfetto is used for the backend instead of TraceLog,
// most of the mojom::Agent functions will never be used
// as the control signals will go through the Perfetto
// interface instead.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventAgent : public BaseAgent {
 public:
  static TraceEventAgent* GetInstance();

  void GetCategories(std::set<std::string>* category_set) override;

  using MetadataGeneratorFunction =
      base::RepeatingCallback<std::unique_ptr<base::DictionaryValue>()>;
  void AddMetadataGeneratorFunction(MetadataGeneratorFunction generator);

 private:
  friend base::NoDestructor<tracing::TraceEventAgent>;
  friend std::default_delete<TraceEventAgent>;      // For Testing
  friend class TraceEventAgentTest;                 // For Testing

  TraceEventAgent();
  ~TraceEventAgent() override;

  // mojom::Agent
  void StartTracing(const std::string& config,
                    base::TimeTicks coordinator_time,
                    StartTracingCallback callback) override;
  void StopAndFlush(mojo::PendingRemote<mojom::Recorder> recorder) override;

  void RequestBufferStatus(RequestBufferStatusCallback callback) override;

  void OnTraceLogFlush(const scoped_refptr<base::RefCountedString>& events_str,
                       bool has_more_events);

  uint8_t enabled_tracing_modes_;
  mojo::Remote<mojom::Recorder> recorder_;
  std::vector<MetadataGeneratorFunction> metadata_generator_functions_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<TraceEventAgent> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(TraceEventAgent);
};

}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_
