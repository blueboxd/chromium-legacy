// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace WTF {

class String;

}  // namespace WTF

namespace blink {

class ScriptState;
class WakeLockRequestOptions;

class WakeLock final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScriptPromise requestPermission(ScriptState*, const WTF::String& type);
  static ScriptPromise request(ScriptState*,
                               const WTF::String& type,
                               WakeLockRequestOptions*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_
