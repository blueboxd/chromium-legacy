// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentMethodData;

import java.util.Map;

/**
 * The parameters of PaymentRequest specified by the merchant.
 */
public interface PaymentRequestParams {
    /** @return The requestShipping set by the merchant. */
    boolean requestShipping();

    /** @return The requestPayerName set by the merchant.  */
    boolean requestPayerName();

    /** @return The requestPayerEmail set by the merchant.  */
    boolean requestPayerEmail();

    /** @return The requestPayerPhone set by the merchant.  */
    boolean requestPayerPhone();

    /**
     * @return The unmodifiable mapping of payment method identifier to the method-specific data in
     * the payment request.
     */
    Map<String, PaymentMethodData> getMethodDataMap();
}
