// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class KeyboardLayout;
class KeyboardLock;
class ScriptState;

class Keyboard final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Keyboard(ExecutionContext*);
  ~Keyboard() override;

  // KeyboardLock API: https://w3c.github.io/keyboard-lock/
  ScriptPromise lock(ScriptState*, const Vector<String>&, ExceptionState&);
  void unlock(ScriptState*);

  ScriptPromise getLayoutMap(ScriptState*, ExceptionState&);

  // ScriptWrappable override.
  void Trace(Visitor*) override;

 private:
  Member<KeyboardLock> keyboard_lock_;
  Member<KeyboardLayout> keyboard_layout_;

  DISALLOW_COPY_AND_ASSIGN(Keyboard);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_H_
