// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

ExecutionContextClient::ExecutionContextClient(
    ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

ExecutionContextClient::ExecutionContextClient(LocalFrame* frame)
    : execution_context_(frame ? frame->DomWindow() : nullptr) {}

ExecutionContext* ExecutionContextClient::GetExecutionContext() const {
  return execution_context_ && !execution_context_->IsContextDestroyed()
             ? execution_context_.Get()
             : nullptr;
}

LocalDOMWindow* ExecutionContextClient::DomWindow() const {
  return DynamicTo<LocalDOMWindow>(GetExecutionContext());
}

LocalFrame* ExecutionContextClient::GetFrame() const {
  auto* window = DomWindow();
  return window ? window->GetFrame() : nullptr;
}

void ExecutionContextClient::Trace(Visitor* visitor) {
  visitor->Trace(execution_context_);
}

ExecutionContextLifecycleObserver::ExecutionContextLifecycleObserver(
    ExecutionContext* execution_context,
    Type type)
    : observer_type_(type) {
  SetExecutionContext(execution_context);
}

ExecutionContext* ExecutionContextLifecycleObserver::GetExecutionContext()
    const {
  return static_cast<ExecutionContext*>(GetContextLifecycleNotifier());
}

void ExecutionContextLifecycleObserver::SetExecutionContext(
    ExecutionContext* execution_context) {
  SetContextLifecycleNotifier(execution_context);
}

LocalFrame* ExecutionContextLifecycleObserver::GetFrame() const {
  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  return window ? window->GetFrame() : nullptr;
}

void ExecutionContextLifecycleObserver::Trace(Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
