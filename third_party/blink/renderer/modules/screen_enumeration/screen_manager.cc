// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/screen_manager.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/screen_enumeration/display.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

void DidReceiveDisplays(ScriptPromiseResolver* resolver,
                        WTF::Vector<mojom::blink::DisplayPtr> backend_displays,
                        bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  HeapVector<Member<Display>> displays;
  displays.ReserveInitialCapacity(backend_displays.size());
  for (const auto& backend_display : backend_displays) {
    auto* display = MakeGarbageCollected<Display>();
    display->setName(backend_display->name);
    display->setScaleFactor(backend_display->scale_factor);
    display->setWidth(backend_display->width);
    display->setHeight(backend_display->height);
    display->setLeft(backend_display->left);
    display->setTop(backend_display->top);
    display->setColorDepth(backend_display->color_depth);
    display->setIsPrimary(backend_display->is_primary);
    display->setIsInternal(backend_display->is_internal);
    displays.emplace_back(display);
  }
  resolver->Resolve(std::move(displays));
}

}  // namespace

ScreenManager::ScreenManager(
    mojo::Remote<mojom::blink::ScreenEnumeration> backend)
    : backend_(std::move(backend)) {
  backend_.set_disconnect_handler(WTF::Bind(
      &ScreenManager::OnBackendDisconnected, WrapWeakPersistent(this)));
}

ScriptPromise ScreenManager::requestDisplays(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  if (!backend_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ScreenManager backend went away");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->RequestDisplays(
      WTF::Bind(&DidReceiveDisplays, WrapPersistent(resolver)));

  return resolver->Promise();
}

void ScreenManager::OnBackendDisconnected() {
  backend_.reset();
}

}  // namespace blink
