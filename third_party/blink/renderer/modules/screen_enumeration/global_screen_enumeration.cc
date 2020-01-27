// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/global_screen_enumeration.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/screen_enumeration/screen_enumeration.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/display/mojom/display.mojom-blink.h"

namespace blink {

namespace {

void DidGetDisplays(
    ScriptPromiseResolver* resolver,
    mojo::Remote<mojom::blink::ScreenEnumeration>,
    WTF::Vector<display::mojom::blink::DisplayPtr> backend_displays,
    int64_t internal_id,
    int64_t primary_id,
    bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  HeapVector<Member<Screen>> screens;
  screens.ReserveInitialCapacity(backend_displays.size());
  for (display::mojom::blink::DisplayPtr& backend_display : backend_displays) {
    const bool internal = backend_display->id == internal_id;
    const bool primary = backend_display->id == primary_id;
    screens.emplace_back(MakeGarbageCollected<Screen>(
        std::move(backend_display), internal, primary));
  }
  resolver->Resolve(std::move(screens));
}

}  // namespace

// static
ScriptPromise GlobalScreenEnumeration::getScreens(
    ScriptState* script_state,
    LocalDOMWindow&,
    ExceptionState& exception_state) {
  // TODO(msw): Cache the backend connection.
  mojo::Remote<mojom::blink::ScreenEnumeration> backend;
  ExecutionContext::From(script_state)
      ->GetBrowserInterfaceBroker()
      .GetInterface(backend.BindNewPipeAndPassReceiver());

  if (!backend) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ScreenEnumeration backend unavailable");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* raw_backend = backend.get();
  raw_backend->GetDisplays(
      WTF::Bind(&DidGetDisplays, WrapPersistent(resolver), std::move(backend)));
  return resolver->Promise();
}

}  // namespace blink
