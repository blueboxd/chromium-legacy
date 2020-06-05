// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"

#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-blink.h"
#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"

namespace mojo {

blink::ItemDetails*
TypeConverter<blink::ItemDetails*, payments::mojom::blink::ItemDetailsPtr>::
    Convert(const payments::mojom::blink::ItemDetailsPtr& input) {
  if (!input)
    return nullptr;
  blink::ItemDetails* output = blink::ItemDetails::Create();
  output->setItemId(input->item_id);
  output->setTitle(input->title);
  output->setDescription(input->description);
  output->setPrice(
      blink::PaymentEventDataConversion::ToPaymentCurrencyAmount(input->price));
  return output;
}

WTF::String
TypeConverter<WTF::String, payments::mojom::blink::BillingResponseCode>::
    Convert(const payments::mojom::blink::BillingResponseCode& input) {
  switch (input) {
    case payments::mojom::blink::BillingResponseCode::kOk:
      return "ok";
    case payments::mojom::blink::BillingResponseCode::kError:
      return "error";
    case payments::mojom::blink::BillingResponseCode::kBillingUnavailable:
      return "billingUnavailable";
    case payments::mojom::blink::BillingResponseCode::kDeveloperError:
      return "developerError";
    case payments::mojom::blink::BillingResponseCode::kFeatureNotSupported:
      return "featureNotSupported";
    case payments::mojom::blink::BillingResponseCode::kItemAlreadyOwned:
      return "itemAlreadyOwned";
    case payments::mojom::blink::BillingResponseCode::kItemNotOwned:
      return "itemNotOwned";
    case payments::mojom::blink::BillingResponseCode::kItemUnavailable:
      return "itemUnavailable";
    case payments::mojom::blink::BillingResponseCode::kServiceDisconnected:
      return "serviceDisconnected";
    case payments::mojom::blink::BillingResponseCode::kServiceUnavailable:
      return "serviceUnavailable";
    case payments::mojom::blink::BillingResponseCode::kUserCancelled:
      return "userCancelled";
  }

  NOTREACHED();
}

}  // namespace mojo
