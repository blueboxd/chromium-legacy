// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/digital_goods_service.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

namespace blink {

DigitalGoodsService::DigitalGoodsService(ExecutionContext* context) {}

DigitalGoodsService::~DigitalGoodsService() = default;

ScriptPromise DigitalGoodsService::getDetails(ScriptState* script_state,
                                              const Vector<String>& item_ids) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO(crbug.com/1061503): This should call out to a mojo service. However,
  // we can't land the mojo service until a browser side implementation is
  // available (for security review). Until then, use this stub which never
  // resolves.

  return resolver->Promise();
}

ScriptPromise DigitalGoodsService::consume(ScriptState* script_state,
                                           const String& purchase_token,
                                           bool make_available_again) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO(crbug.com/1061503): This should call out to a mojo service. However,
  // we can't land the mojo service until a browser side implementation is
  // available (for security review). Until then, use this stub which never
  // resolves.

  return resolver->Promise();
}

void DigitalGoodsService::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
