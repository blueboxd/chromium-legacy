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

ContextLifecycleObserver::ContextLifecycleObserver(Document* document,
                                                   Type type)
    : LifecycleObserver(document ? document->ToExecutionContext() : nullptr),
      observer_type_(type) {}

LocalFrame* ContextLifecycleObserver::GetFrame() const {
  auto* document = Document::DynamicFrom(GetExecutionContext());
  return document ? document->GetFrame() : nullptr;
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
