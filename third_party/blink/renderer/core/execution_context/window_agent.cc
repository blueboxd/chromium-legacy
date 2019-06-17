// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent.h"

#include "third_party/blink/renderer/core/dom/mutation_observer_notifier.h"

namespace blink {

WindowAgent::WindowAgent(v8::Isolate* isolate)
    : Agent(isolate),
      mutation_observer_notifier_(
          MakeGarbageCollected<MutationObserverNotifier>()) {}

WindowAgent::~WindowAgent() = default;

void WindowAgent::Trace(Visitor* visitor) {
  Agent::Trace(visitor);
  visitor->Trace(mutation_observer_notifier_);
}

}  // namespace blink
