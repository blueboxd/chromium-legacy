// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.payments.mojom.PaymentAddress;

/**
 * A class used to bundle the payer data received from payment handlers.
 */
public class PayerData {
    public final String payerName;
    public final String payerPhone;
    public final String payerEmail;
    public final PaymentAddress shippingAddress;
    public final String selectedShippingOptionId;

    public PayerData(String payerName, String payerPhone, String payerEmail,
            PaymentAddress shippingAddress, String selectedShippingOptionId) {
        this.payerName = payerName;
        this.payerPhone = payerPhone;
        this.payerEmail = payerEmail;
        this.shippingAddress = shippingAddress;
        this.selectedShippingOptionId = selectedShippingOptionId;
    }

    public PayerData() {
        payerName = null;
        payerPhone = null;
        payerEmail = null;
        shippingAddress = null;
        selectedShippingOptionId = null;
    }
}
