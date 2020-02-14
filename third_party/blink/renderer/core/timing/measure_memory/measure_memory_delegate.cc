// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_delegate.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory_breakdown.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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

String GetOrigin(v8::Local<v8::Context> context) {
  ExecutionContext* execution_context = ExecutionContext::From(context);
  if (!execution_context) {
    // TODO(ulan): Store URL in v8::Context, so that it is available
    // event for detached contexts.
    return String("detached");
  }
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityContext().GetSecurityOrigin();
  return security_origin->ToString();
}

MeasureMemoryBreakdown* CreateMeasureMemoryBreakdown(size_t bytes,
                                                     size_t globals,
                                                     const String& type,
                                                     const String& origin) {
  MeasureMemoryBreakdown* result = MeasureMemoryBreakdown::Create();
  result->setBytes(bytes);
  result->setGlobals(globals);
  result->setType(type);
  result->setOrigins(Vector<String>{origin});
  return result;
}

struct BytesAndGlobals {
  size_t bytes;
  size_t globals;
};

HashMap<String, BytesAndGlobals> GroupByOrigin(
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
        context_sizes) {
  HashMap<String, BytesAndGlobals> per_origin;
  for (const auto& context_size : context_sizes) {
    const String origin = GetOrigin(context_size.first);
    auto it = per_origin.find(origin);
    if (it == per_origin.end()) {
      per_origin.insert(origin, BytesAndGlobals{context_size.second, 1});
    } else {
      it->value.bytes += context_size.second;
      ++it->value.globals;
    }
  }
  return per_origin;
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
  ExecutionContext* execution_context = ExecutionContext::From(context);
  if (!execution_context) {
    // The context was detached in the meantime.
    return;
  }
  v8::Context::Scope context_scope(context);
  size_t total_size = 0;
  for (const auto& context_size : context_sizes) {
    total_size += context_size.second;
  }
  MeasureMemory* result = MeasureMemory::Create();
  result->setBytes(total_size + unattributed_size);
  HeapVector<Member<MeasureMemoryBreakdown>> breakdown;
  HashMap<String, BytesAndGlobals> per_origin(GroupByOrigin(context_sizes));
  for (const auto& it : per_origin) {
    breakdown.push_back(CreateMeasureMemoryBreakdown(
        it.value.bytes, it.value.globals, "js", it.key));
  }
  breakdown.push_back(CreateMeasureMemoryBreakdown(
      unattributed_size, context_sizes.size(), "js", "shared"));
  result->setBreakdown(breakdown);
  v8::Local<v8::Promise::Resolver> promise_resolver =
      promise_resolver_.NewLocal(isolate_);
  promise_resolver->Resolve(context, ToV8(result, promise_resolver, isolate_))
      .ToChecked();
}

}  // namespace blink
