// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/ink.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ScriptPromise Ink::requestPresenter(ScriptState* state,
                                    String type,
                                    Element* presentationArea) {
  DCHECK(RuntimeEnabledFeatures::DelegatedInkTrailsEnabled());
  DCHECK_EQ(type, "delegated-ink-trail");

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(state);
  ScriptPromise promise = resolver->Promise();

  if (!state->ContextIsValid()) {
    resolver->Reject(V8ThrowException::CreateError(
        state->GetIsolate(), "Unable to create presenter"));
    return promise;
  }

  if (type != "delegated-ink-trail") {
    resolver->Reject(
        V8ThrowException::CreateTypeError(state->GetIsolate(), "Bad type"));
    return promise;
  }

  DelegatedInkTrailPresenter* trail_presenter =
      MakeGarbageCollected<DelegatedInkTrailPresenter>(presentationArea);

  resolver->Resolve(trail_presenter);
  return promise;
}

void Ink::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
