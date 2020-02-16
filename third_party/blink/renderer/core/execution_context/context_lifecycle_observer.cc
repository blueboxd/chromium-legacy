// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

ContextClient::ContextClient(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

ContextClient::ContextClient(LocalFrame* frame)
    : execution_context_(frame ? frame->GetDocument()->ToExecutionContext()
                               : nullptr) {}

ExecutionContext* ContextClient::GetExecutionContext() const {
  return execution_context_ && !execution_context_->IsContextDestroyed()
             ? execution_context_.Get()
             : nullptr;
}

Document* ContextClient::GetDocument() const {
  return execution_context_
             ? Document::DynamicFrom(
                   static_cast<ExecutionContext*>(execution_context_))
             : nullptr;
}

LocalFrame* ContextClient::GetFrame() const {
  auto* document = GetDocument();
  return document ? document->GetFrame() : nullptr;
}

void ContextClient::Trace(Visitor* visitor) {
  visitor->Trace(execution_context_);
}

ContextLifecycleObserver::ContextLifecycleObserver()
    : observer_type_(kGenericType) {}

ContextLifecycleObserver::ContextLifecycleObserver(Document* document,
                                                   Type type)
    : ContextLifecycleObserver(
          document ? document->ToExecutionContext() : nullptr,
          type) {}

ContextLifecycleObserver::ContextLifecycleObserver(
    ExecutionContext* execution_context,
    Type type)
    : observer_type_(type) {
  SetExecutionContext(execution_context);
}

void ContextLifecycleObserver::ObserverListWillBeCleared() {
  execution_context_ = nullptr;
}

void ContextLifecycleObserver::SetExecutionContext(
    ExecutionContext* execution_context) {
  if (execution_context == execution_context_)
    return;

  if (execution_context_)
    execution_context_->ContextLifecycleObserverList().RemoveObserver(this);

  execution_context_ = execution_context;

  if (execution_context_)
    execution_context_->ContextLifecycleObserverList().AddObserver(this);
}

LocalFrame* ContextLifecycleObserver::GetFrame() const {
  auto* document = Document::DynamicFrom(GetExecutionContext());
  return document ? document->GetFrame() : nullptr;
}

void ContextLifecycleObserver::Trace(Visitor* visitor) {
  visitor->Trace(execution_context_);
}

DOMWindowClient::DOMWindowClient(LocalDOMWindow* window)
    : dom_window_(window) {}

DOMWindowClient::DOMWindowClient(LocalFrame* frame)
    : dom_window_(frame ? frame->DomWindow() : nullptr) {}

LocalDOMWindow* DOMWindowClient::DomWindow() const {
  return dom_window_ && dom_window_->GetFrame() ? dom_window_ : nullptr;
}

LocalFrame* DOMWindowClient::GetFrame() const {
  return dom_window_ ? dom_window_->GetFrame() : nullptr;
}

void DOMWindowClient::Trace(Visitor* visitor) {
  visitor->Trace(dom_window_);
}
}  // namespace blink
