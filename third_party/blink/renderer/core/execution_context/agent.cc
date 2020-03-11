// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue)
    : event_loop_(base::AdoptRef(
          new scheduler::EventLoop(isolate, std::move(microtask_queue)))),
      cluster_id_(cluster_id) {}

Agent::~Agent() = default;

void Agent::Trace(Visitor* visitor) {}

void Agent::AttachDocument(Document* document) {
  event_loop_->AttachScheduler(document->GetScheduler());
}

void Agent::DetachDocument(Document* document) {
  event_loop_->DetachScheduler(document->GetScheduler());
}

}  // namespace blink
