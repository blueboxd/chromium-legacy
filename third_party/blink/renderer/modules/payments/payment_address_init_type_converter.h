// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_ADDRESS_INIT_TYPE_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_ADDRESS_INIT_TYPE_CONVERTER_H_

#include "components/payments/mojom/payment_request_data.mojom-blink.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/renderer/modules/payments/payment_address_init.h"

namespace mojo {

template <>
struct TypeConverter<payments::mojom::blink::PaymentAddressPtr,
                     blink::PaymentAddressInit*> {
  static payments::mojom::blink::PaymentAddressPtr Convert(
      const blink::PaymentAddressInit* input);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_ADDRESS_INIT_TYPE_CONVERTER_H_
