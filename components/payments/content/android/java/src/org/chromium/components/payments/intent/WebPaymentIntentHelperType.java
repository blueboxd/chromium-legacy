// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.intent;

/**
 * The types that mirror the corresponding types in org.chromium.payments.mojom. This class should
 * be independent of the org.chromium package.
 */
public final class WebPaymentIntentHelperType {
    /** The class that mirrors mojom.PaymentCurrencyAmount. */
    public static final class PaymentCurrencyAmount {
        public final String currency;
        public final String value;
        public PaymentCurrencyAmount(String currency, String value) {
            this.currency = currency;
            this.value = value;
        }
    }

    /** The class that mirrors mojom.PaymentItem. */
    public static final class PaymentItem {
        public final PaymentCurrencyAmount amount;
        public PaymentItem(PaymentCurrencyAmount amount) {
            this.amount = amount;
        }
    }

    /** The class that mirrors mojom.PaymentDetailsModifier. */
    public static final class PaymentDetailsModifier {
        public final PaymentItem total;
        public final PaymentMethodData methodData;
        public PaymentDetailsModifier(PaymentItem total, PaymentMethodData methodData) {
            this.total = total;
            this.methodData = methodData;
        }
    }

    /** The class that mirrors mojom.PaymentMethodData. */
    public static final class PaymentMethodData {
        public final String supportedMethod;
        public final String stringifiedData;
        public PaymentMethodData(String supportedMethod, String stringifiedData) {
            this.supportedMethod = supportedMethod;
            this.stringifiedData = stringifiedData;
        }
    }
}
