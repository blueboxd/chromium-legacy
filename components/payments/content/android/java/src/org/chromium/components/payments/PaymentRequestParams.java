// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.Map;

/**
 * The parameters of PaymentRequest specified by the merchant.
 */
public interface PaymentRequestParams {
    /** @return The PaymentOptions set by the merchant.  */
    PaymentOptions getPaymentOptions();

    /**
     * @return The unmodifiable mapping of payment method identifier to the method-specific data in
     * the payment request.
     */
    Map<String, PaymentMethodData> getMethodData();
}
