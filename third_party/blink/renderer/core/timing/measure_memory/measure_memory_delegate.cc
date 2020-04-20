// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_delegate.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory_breakdown.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

MeasureMemoryDelegate::MeasureMemoryDelegate(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Promise::Resolver> promise_resolver)
    : isolate_(isolate),
      context_(isolate, context),
      promise_resolver_(isolate, promise_resolver) {
  context_.SetPhantom();
  // TODO(ulan): Currently we keep a strong reference to the promise resolver.
  // This may prolong the lifetime of the context by one more GC in the worst
  // case as JSPromise keeps its context alive.
  // To avoid that we should store the promise resolver in V8PerContextData.
}

// Returns true if the given context should be included in the current memory
// measurement. Currently it is very conservative and allows only the same
// origin contexts that belong to the same JavaScript origin.
// With COOP/COEP we will be able to relax this restriction for the contexts
// that opt-in into memory measurement.
bool MeasureMemoryDelegate::ShouldMeasure(v8::Local<v8::Context> context) {
  if (context_.IsEmpty()) {
    // The original context was garbage collected in the meantime.
    return false;
  }
  v8::Local<v8::Context> original_context = context_.NewLocal(isolate_);
  ExecutionContext* original_execution_context =
      ExecutionContext::From(original_context);
  ExecutionContext* execution_context = ExecutionContext::From(context);
  if (!original_execution_context || !execution_context) {
    // One of the contexts is detached or is created by DevTools.
    return false;
  }
  if (original_execution_context->GetAgent() != execution_context->GetAgent()) {
    // Context do not belong to the same JavaScript agent.
    return false;
  }
  if (ScriptState::From(context)->World().IsIsolatedWorld()) {
    // Context belongs to an extension. Skip it.
    return false;
  }
  const SecurityOrigin* original_security_origin =
      original_execution_context->GetSecurityContext().GetSecurityOrigin();
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityContext().GetSecurityOrigin();
  if (!original_security_origin->IsSameOriginWith(security_origin)) {
    // TODO(ulan): Check for COOP/COEP and allow cross-origin contexts that
    // opted in for memory measurement.
    // Until then we allow cross-origin measurement only for site-isolated
    // web pages.
    return Platform::Current()->IsLockedToSite();
  }
  return true;
}

namespace {
// Helper functions for constructing a memory measurement result.

const LocalFrame* GetFrame(v8::Local<v8::Context> context) {
  LocalDOMWindow* window = ToLocalDOMWindow(context);
  if (!window) {
    // The context was detached. Ignore it.
    return nullptr;
  }
  return window->GetFrame();
}

String GetUrl(const LocalFrame* frame) {
  if (frame->IsCrossOriginToParentFrame()) {
    // The function must be called only for the first cross-origin iframe on
    // the path down from the main frame. Thus the parent frame is guaranteed
    // to be the same origin as the main frame.
    DCHECK(!frame->Tree().Parent() ||
           !frame->Tree().Parent()->IsCrossOriginToMainFrame());
    base::Optional<String> url = frame->FirstUrlCrossOriginToParent();
    return url ? url.value() : "";
  }
  return frame->GetDocument()->Url().GetString();
}

// To avoid information leaks cross-origin iframes are considered opaque for
// the purposes of attribution. This means the memory of all iframes nested
// in a cross-origin iframe is attributed to the cross-origin iframe.
// See https://github.com/WICG/performance-measure-memory for more details.
//
// Given the main frame and the current context, this function walks up the
// tree and finds the topmost cross-origin ancestor frame in the path.
// If that doesn't exist, then all frames in the path are same-origin,
// so the frame corresponding to the current context is returned.
//
// The function returns nullptr if the context was detached.
const LocalFrame* GetAttributionFrame(const LocalFrame* main_frame,
                                      v8::Local<v8::Context> context) {
  const LocalFrame* frame = GetFrame(context);
  if (!frame) {
    // The context was detached. Ignore it.
    return nullptr;
  }
  if (&frame->Tree().Top() != main_frame) {
    // This can happen if the frame was detached.
    // See the comment in FrameTree::Top().
    return nullptr;
  }
  // Walk up the tree and find the topmost cross-origin ancestor frame.
  const LocalFrame* result = frame;
  // The parent is guaranteed to be LocalFrame because |frame| and
  // |main_frame| belong to the same JS agent.
  frame = To<LocalFrame>(frame->Tree().Parent());
  while (frame) {
    if (frame->IsCrossOriginToMainFrame())
      result = frame;
    frame = To<LocalFrame>(frame->Tree().Parent());
  }
  return result;
}

// Return per-frame sizes based on the given per-context size.
// TODO(ulan): Revisit this after Origin Trial and see if the results
// are precise enough or if we need to additionally group by JS agent.
HeapHashMap<Member<const LocalFrame>, size_t> GroupByFrame(
    const LocalFrame* main_frame,
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
        context_sizes) {
  HeapHashMap<Member<const LocalFrame>, size_t> per_frame;
  for (const auto& context_size : context_sizes) {
    const LocalFrame* frame =
        GetAttributionFrame(main_frame, context_size.first);
    if (!frame) {
      // The context was detached. Ignore it.
      continue;
    }
    auto it = per_frame.find(frame);
    if (it == per_frame.end()) {
      per_frame.insert(frame, context_size.second);
    } else {
      it->value += context_size.second;
    }
  }
  return per_frame;
}

MeasureMemoryBreakdown* CreateMeasureMemoryBreakdown(
    size_t bytes,
    const Vector<String>& types,
    const String& url) {
  MeasureMemoryBreakdown* result = MeasureMemoryBreakdown::Create();
  result->setBytes(bytes);
  result->setUserAgentSpecificTypes(types);
  result->setAttribution(url.length() ? Vector<String>{url} : Vector<String>());
  return result;
}

}  // anonymous namespace

// Constructs a memory measurement result based on the given list of (context,
// size) pairs and resolves the promise.
void MeasureMemoryDelegate::MeasurementComplete(
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>& context_sizes,
    size_t unattributed_size) {
  if (context_.IsEmpty()) {
    // The context was garbage collected in the meantime.
    return;
  }
  v8::Local<v8::Context> context = context_.NewLocal(isolate_);
  const LocalFrame* frame = GetFrame(context);
  if (!frame) {
    // The context was detached in the meantime.
    return;
  }
  DCHECK(frame->IsMainFrame());
  v8::Context::Scope context_scope(context);
  size_t total_size = 0;
  for (const auto& context_size : context_sizes) {
    total_size += context_size.second;
  }
  MeasureMemory* result = MeasureMemory::Create();
  result->setBytes(total_size + unattributed_size);
  HeapVector<Member<MeasureMemoryBreakdown>> breakdown;
  HeapHashMap<Member<const LocalFrame>, size_t> per_frame(
      GroupByFrame(frame, context_sizes));
  size_t attributed_size = 0;
  const String kWindow("Window");
  const String kJS("JS");
  for (const auto& it : per_frame) {
    attributed_size += it.value;
    breakdown.push_back(CreateMeasureMemoryBreakdown(
        it.value, Vector<String>{kWindow, kJS}, GetUrl(it.key)));
  }
  const String kDetached("Detached");
  const String kShared("Shared");
  size_t detached_size = total_size - attributed_size;
  breakdown.push_back(CreateMeasureMemoryBreakdown(
      detached_size, Vector<String>{kWindow, kJS, kDetached}, ""));
  breakdown.push_back(CreateMeasureMemoryBreakdown(
      unattributed_size, Vector<String>{kWindow, kJS, kShared}, ""));
  result->setBreakdown(breakdown);
  v8::Local<v8::Promise::Resolver> promise_resolver =
      promise_resolver_.NewLocal(isolate_);
  promise_resolver->Resolve(context, ToV8(result, promise_resolver, isolate_))
      .ToChecked();
}

}  // namespace blink
