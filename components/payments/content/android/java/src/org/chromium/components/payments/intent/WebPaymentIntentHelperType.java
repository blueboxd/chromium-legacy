// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.intent;

/**
 * The types that corresponds to the types in org.chromium.payments.mojom. The fields of these types
 * are the subset of those in the mojom types. The subset is minimally selected based on the need of
 * this package. This class should be independent of the org.chromium package.
 *
 * @see <a
 *         href="https://developers.google.com/web/fundamentals/payments/payment-apps-developer-guide/android-payment-apps#payment_parameters">Payment
 *         parameters</a>
 * @see <a
 *         href="https://developers.google.com/web/fundamentals/payments/payment-apps-developer-guide/android-payment-apps#%E2%80%9Cis_ready_to_pay%E2%80%9D_parameters">“Is
 *         ready to pay” parameters</a>
 */
public final class WebPaymentIntentHelperType {
    /**
     * The class that corresponds to mojom.PaymentCurrencyAmount, with minimally required fields.
     */
    public static final class PaymentCurrencyAmount {
        public final String currency;
        public final String value;
        public PaymentCurrencyAmount(String currency, String value) {
            this.currency = currency;
            this.value = value;
        }
    }

    /** The class that corresponds mojom.PaymentItem, with minimally required fields. */
    public static final class PaymentItem {
        public final PaymentCurrencyAmount amount;
        public PaymentItem(PaymentCurrencyAmount amount) {
            this.amount = amount;
        }
    }

    /** The class that corresponds mojom.PaymentDetailsModifier, with minimally required fields. */
    public static final class PaymentDetailsModifier {
        public final PaymentItem total;
        public final PaymentMethodData methodData;
        public PaymentDetailsModifier(PaymentItem total, PaymentMethodData methodData) {
            this.total = total;
            this.methodData = methodData;
        }
    }

    /** The class that corresponds mojom.PaymentMethodData, with minimally required fields. */
    public static final class PaymentMethodData {
        public final String supportedMethod;
        public final String stringifiedData;
        public PaymentMethodData(String supportedMethod, String stringifiedData) {
            this.supportedMethod = supportedMethod;
            this.stringifiedData = stringifiedData;
        }
    }
}
