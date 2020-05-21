// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DIGITAL_GOODS_SERVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DIGITAL_GOODS_SERVICE_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"

namespace blink {

class ExecutionContext;
class ScriptState;
class Visitor;

class DigitalGoodsService final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit DigitalGoodsService(ExecutionContext* context);
  ~DigitalGoodsService() override;

  // IDL Interface:
  const String& paymentMethod() const { return payment_method_; }
  ScriptPromise getDetails(ScriptState*, const Vector<String>& item_ids);
  ScriptPromise consume(ScriptState*,
                        const String& purchase_token,
                        bool make_available_again);

  void Trace(Visitor* visitor) const override;

 private:
  const String payment_method_ = "https://some.url/for/payment/request/api";
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DIGITAL_GOODS_SERVICE_H_
