// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
ScriptIterator ScriptIterator::FromIterable(v8::Isolate* isolate,
                                            v8::Local<v8::Object> value,
                                            ExceptionState& exception_state) {
  // First, call the GetMethod(V, @@iterator) abstract ES operation.
  const v8::Local<v8::Function> iterator_method =
      GetEsIteratorMethod(isolate, value, exception_state);
  if (exception_state.HadException())
    return ScriptIterator();
  if (iterator_method.IsEmpty())
    return ScriptIterator();

  // Use the method returned above to invoke the GetIterator(V, sync, method)
  // abstract ES operation.
  const v8::Local<v8::Object> iterator =
      GetEsIteratorWithMethod(isolate, iterator_method, value, exception_state);
  if (exception_state.HadException())
    return ScriptIterator();

  return ScriptIterator(isolate, iterator);
}

ScriptIterator::ScriptIterator(v8::Isolate* isolate,
                               v8::Local<v8::Object> iterator)
    : isolate_(isolate),
      iterator_(iterator),
      next_key_(V8AtomicString(isolate, "next")),
      done_key_(V8AtomicString(isolate, "done")),
      value_key_(V8AtomicString(isolate, "value")),
      done_(false) {
  DCHECK(!iterator.IsEmpty());
}

bool ScriptIterator::Next(ExecutionContext* execution_context,
                          ExceptionState& exception_state,
                          v8::Local<v8::Value> next_value) {
  DCHECK(!IsNull());

  v8::TryCatch try_catch(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Value> next;
  if (!iterator_->Get(context, next_key_).ToLocal(&next)) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }
  if (!next->IsFunction()) {
    exception_state.ThrowTypeError("Expected next() function on iterator.");
    done_ = true;
    return false;
  }

  Vector<v8::Local<v8::Value>, 1> argv;
  if (!next_value.IsEmpty())
    argv = {next_value};

  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::CallFunction(v8::Local<v8::Function>::Cast(next),
                                    execution_context, iterator_, argv.size(),
                                    argv.data(), isolate_)
           .ToLocal(&result)) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }
  if (!result->IsObject()) {
    exception_state.ThrowTypeError(
        "Expected iterator.next() to return an Object.");
    done_ = true;
    return false;
  }
  v8::Local<v8::Object> result_object = v8::Local<v8::Object>::Cast(result);

  value_ = result_object->Get(context, value_key_);
  if (value_.IsEmpty()) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
  }

  v8::Local<v8::Value> done;
  if (!result_object->Get(context, done_key_).ToLocal(&done)) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }

  done_ = done->BooleanValue(isolate_);
  return !done_;
}

}  // namespace blink
