// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/dom_window_digital_goods.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_service.h"

namespace {

// TODO (crbug.com/1061503): Point URL to Play payment request API once known.
const char known_payment_method_[] = "https://some.url/for/payment/request/api";

}  // namespace

namespace blink {

const char DOMWindowDigitalGoods::kSupplementName[] = "DOMWindowDigitalGoods";

ScriptPromise DOMWindowDigitalGoods::getDigitalGoodsService(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const String& payment_method) {
  return FromState(&window)->GetDigitalGoodsService(script_state,
                                                    payment_method);
}

ScriptPromise DOMWindowDigitalGoods::GetDigitalGoodsService(
    ScriptState* script_state,
    const String& payment_method) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  // TODO (crbug.com/1061503): Enable JS to connect to various payment method
  // backends. For now, just connect to one known backend and check the URL is
  // correct for that payment method.
  if (payment_method != known_payment_method_) {
    resolver->Resolve();
    return promise;
  }

  if (!digital_goods_service_) {
    digital_goods_service_ = MakeGarbageCollected<DigitalGoodsService>(
        ExecutionContext::From(script_state));
  }

  resolver->Resolve(digital_goods_service_);
  return promise;
}

void DOMWindowDigitalGoods::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(digital_goods_service_);
}

// static
DOMWindowDigitalGoods* DOMWindowDigitalGoods::FromState(
    LocalDOMWindow* window) {
  DOMWindowDigitalGoods* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowDigitalGoods>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowDigitalGoods>();
    ProvideTo(*window, supplement);
  }

  return supplement;
}

}  // namespace blink
