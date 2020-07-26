// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DOM_WINDOW_DIGITAL_GOODS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DOM_WINDOW_DIGITAL_GOODS_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class DigitalGoodsService;
class LocalDOMWindow;
class ScriptState;
class Visitor;

class DOMWindowDigitalGoods final
    : public GarbageCollected<DOMWindowDigitalGoods>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  // IDL Interface:
  static ScriptPromise getDigitalGoodsService(ScriptState*,
                                              LocalDOMWindow&,
                                              const String& payment_method);

  ScriptPromise GetDigitalGoodsService(ScriptState*,
                                       const String& payment_method);
  void Trace(Visitor* visitor) const override;

 private:
  Member<DigitalGoodsService> digital_goods_service_;

  static DOMWindowDigitalGoods* FromState(LocalDOMWindow*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DOM_WINDOW_DIGITAL_GOODS_H_
