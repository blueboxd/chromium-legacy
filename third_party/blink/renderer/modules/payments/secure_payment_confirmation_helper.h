// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_HELPER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ScriptValue;
class ExceptionState;

class SecurePaymentConfirmationHelper {
  STATIC_ONLY(SecurePaymentConfirmationHelper);

 public:
  // Parse 'secure-payment-confirmation' data in |input| or throw an exception.
  static void ParseSecurePaymentConfirmationData(const ScriptValue& input,
                                                 ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_HELPER_H_
