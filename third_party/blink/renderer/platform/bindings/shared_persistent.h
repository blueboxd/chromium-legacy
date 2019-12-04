/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SHARED_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SHARED_PERSISTENT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "v8/include/v8.h"

namespace blink {

// A ref counted version of ScopedPersistent. This class is intended for use by
// ScriptValue and not for normal use. Consider using ScopedPersistent directly
// instead.
// TODO(crbug.com/1029738): Remove once bug is fixed.
template <typename T>
class SharedPersistent : public RefCounted<SharedPersistent<T>> {
  USING_FAST_MALLOC(SharedPersistent);

 public:
  static scoped_refptr<SharedPersistent<T>> Create(v8::Isolate* isolate,
                                                   v8::Local<T> value) {
    return base::AdoptRef(new SharedPersistent<T>(isolate, value));
  }

  // Returns the V8 reference.  Crashes if |world_| is set and it is
  // different from |target_script_state|'s world.
  v8::Local<T> Get(ScriptState* target_script_state) const {
    DCHECK(!value_.IsEmpty());
    if (world_) {
      CHECK_EQ(world_.get(), &target_script_state->World());
    }
    return value_.NewLocal(target_script_state->GetIsolate());
  }

  // Returns a V8 reference that is safe to access in |target_script_state|.
  // The return value may be a cloned object.
  v8::Local<T> GetAcrossWorld(ScriptState* target_script_state) const {
    CHECK(world_);
    DCHECK(!value_.IsEmpty());

    v8::Isolate* isolate = target_script_state->GetIsolate();

    if (world_ == &target_script_state->World())
      return value_.NewLocal(isolate);

    // If |v8_reference| is a v8::Object, clones |v8_reference| in the context
    // of |target_script_state| and returns it.  Otherwise returns
    // |v8_reference| itself that is already safe to access in
    // |target_script_state|.

    v8::Local<v8::Value> value = value_.NewLocal(isolate);
    DCHECK(value->IsObject());
    return value.template As<T>();
  }

  bool IsEmpty() { return value_.IsEmpty(); }

  bool operator==(const SharedPersistent<T>& other) {
    return value_ == other.value_;
  }

 private:
  explicit SharedPersistent(v8::Isolate* isolate, v8::Local<T> value)
      : value_(isolate, value) {
    DCHECK(!value.IsEmpty());
    // Basically, |world_| is a world when this V8 reference is created.
    // However, when this V8 reference isn't created in context and value is
    // object, we set |world_| to a value's creation cotext's world.
    if (isolate->InContext()) {
      world_ = &DOMWrapperWorld::Current(isolate);
      if (value->IsObject()) {
        v8::Local<v8::Context> context =
            value.template As<v8::Object>()->CreationContext();
        // Creation context is null if the value is a remote object.
        if (!context.IsEmpty()) {
          ScriptState* script_state = ScriptState::From(context);
          CHECK_EQ(world_.get(), &script_state->World());
        }
      }
    } else if (value->IsObject()) {
      ScriptState* script_state =
          ScriptState::From(value.template As<v8::Object>()->CreationContext());
      world_ = &script_state->World();
    }
  }

  ScopedPersistent<T> value_;
  // The world of the current context at the time when |value_| was set.
  // It's guaranteed that, if |value_| is a v8::Object, the world of the
  // creation context of |value_| is the same as |world_|.
  scoped_refptr<const DOMWrapperWorld> world_;

  DISALLOW_COPY_AND_ASSIGN(SharedPersistent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SHARED_PERSISTENT_H_
