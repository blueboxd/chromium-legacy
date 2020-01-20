// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_delegate.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_entry.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_options.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

MeasureMemoryDelegate::MeasureMemoryDelegate(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Promise::Resolver> promise_resolver,
    v8::MeasureMemoryMode mode)
    : isolate_(isolate),
      context_(isolate, context),
      promise_resolver_(isolate, promise_resolver),
      mode_(mode) {
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
  const SecurityOrigin* original_security_origin =
      original_execution_context->GetSecurityContext().GetSecurityOrigin();
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityContext().GetSecurityOrigin();
  if (!original_security_origin->IsSameOriginWith(security_origin)) {
    // TODO(ulan): Check for COOP/COEP and allow cross-origin contexts that
    // opted in for memory measurement.
    return false;
  }
  return true;
}

namespace {
// Helper functions for constructing a memory measurement result.

String GetUrl(v8::Local<v8::Context> context) {
  ExecutionContext* execution_context = ExecutionContext::From(context);
  if (!execution_context) {
    // TODO(ulan): Store URL in v8::Context, so that it is available
    // event for detached contexts.
    return String("detached");
  }
  return execution_context->Url().GetString();
}

MeasureMemoryEntry* CreateMeasureMemoryEntry(size_t estimate,
                                             size_t unattributed) {
  MeasureMemoryEntry* result = MeasureMemoryEntry::Create();
  result->setJSMemoryEstimate(estimate);
  Vector<uint64_t> range;
  range.push_back(estimate);
  range.push_back(estimate + unattributed);
  result->setJSMemoryRange(range);
  return result;
}

MeasureMemoryEntry* CreateMeasureMemoryEntry(size_t estimate,
                                             size_t unattributed,
                                             const String& url) {
  MeasureMemoryEntry* result = CreateMeasureMemoryEntry(estimate, unattributed);
  result->setURL(url);
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
  ExecutionContext* execution_context = ExecutionContext::From(context);
  if (!execution_context) {
    // The context was detached in the meantime.
    return;
  }
  v8::Context::Scope context_scope(context);
  size_t total_size = 0;
  size_t current_size = 0;
  for (const auto& context_size : context_sizes) {
    total_size += context_size.second;
    if (context == context_size.first) {
      current_size = context_size.second;
    }
  }
  MeasureMemory* result = MeasureMemory::Create();
  result->setTotal(CreateMeasureMemoryEntry(total_size, unattributed_size));
  if (mode_ == v8::MeasureMemoryMode::kDetailed) {
    result->setCurrent(CreateMeasureMemoryEntry(current_size, unattributed_size,
                                                GetUrl(context)));
    HeapVector<Member<MeasureMemoryEntry>> other;
    for (const auto& context_size : context_sizes) {
      if (context_size.first == context) {
        // The current context was already reported. Skip it.
        continue;
      }
      other.push_back(CreateMeasureMemoryEntry(
          context_size.second, unattributed_size, GetUrl(context_size.first)));
    }
    result->setOther(other);
  }
  v8::Local<v8::Promise::Resolver> promise_resolver =
      promise_resolver_.NewLocal(isolate_);
  promise_resolver
      ->Resolve(context, result->ToV8Impl(promise_resolver, isolate_))
      .ToChecked();
}

}  // namespace blink
