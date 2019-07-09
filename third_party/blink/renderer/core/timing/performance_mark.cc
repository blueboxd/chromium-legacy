// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/timing/performance_mark.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_mark_options.h"
#include "third_party/blink/renderer/core/timing/performance_user_timing.h"

namespace blink {

PerformanceMark::PerformanceMark(
    ScriptState* script_state,
    const AtomicString& name,
    double start_time,
    scoped_refptr<SerializedScriptValue> serialized_detail,
    ExceptionState& exception_state)
    : PerformanceEntry(name, start_time, start_time),
      serialized_detail_(std::move(serialized_detail)) {}

// static
PerformanceMark* PerformanceMark::Create(ScriptState* script_state,
                                         const AtomicString& name,
                                         double start_time,
                                         const ScriptValue& detail,
                                         ExceptionState& exception_state) {
  scoped_refptr<SerializedScriptValue> serialized_detail;
  if (detail.IsEmpty()) {
    serialized_detail = SerializedScriptValue::NullValue();
  } else {
    serialized_detail = SerializedScriptValue::Serialize(
        script_state->GetIsolate(), detail.V8Value(),
        SerializedScriptValue::SerializeOptions(), exception_state);
    if (exception_state.HadException())
      return nullptr;
  }
  return MakeGarbageCollected<PerformanceMark>(script_state, name, start_time,
                                               std::move(serialized_detail),
                                               exception_state);
}

// static
PerformanceMark* PerformanceMark::Create(ScriptState* script_state,
                                         const AtomicString& mark_name,
                                         PerformanceMarkOptions* mark_options,
                                         ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window)
    return nullptr;
  Performance* performance = DOMWindowPerformance::performance(*window);
  if (!performance)
    return nullptr;

  return performance->GetUserTiming().CreatePerformanceMark(
      script_state, mark_name, mark_options, exception_state);
}

AtomicString PerformanceMark::entryType() const {
  return performance_entry_names::kMark;
}

PerformanceEntryType PerformanceMark::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMark;
}

ScriptValue PerformanceMark::detail(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!serialized_detail_)
    return ScriptValue(script_state, v8::Null(isolate));
  auto result = deserialized_detail_map_.insert(
      script_state, TraceWrapperV8Reference<v8::Value>());
  TraceWrapperV8Reference<v8::Value>& relevant_data =
      result.stored_value->value;
  if (!result.is_new_entry)
    return ScriptValue(script_state, relevant_data.NewLocal(isolate));
  v8::Local<v8::Value> value = serialized_detail_->Deserialize(isolate);
  relevant_data.Set(isolate, value);
  return ScriptValue(script_state, value);
}

void PerformanceMark::Trace(blink::Visitor* visitor) {
  visitor->Trace(deserialized_detail_map_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
