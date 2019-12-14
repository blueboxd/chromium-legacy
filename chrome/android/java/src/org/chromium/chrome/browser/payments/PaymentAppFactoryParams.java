// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.payments.PaymentApp.PaymentRequestUpdateEventCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.Map;

/** Interface for providing information to a payment app factory. */
public interface PaymentAppFactoryParams {
    /** @return The web contents where the payment is being requested. */
    WebContents getWebContents();

    /**
     * @return The unmodifiable mapping of payment method identifier to the method-specific data in
     * the payment request.
     */
    Map<String, PaymentMethodData> getMethodData();

    /** @return The PaymentRequest object identifier. */
    default String getId() {
        return null;
    }

    /**
     * @return The scheme, host, and port of the last committed URL of the top-level context as
     * formatted by UrlFormatter.formatUrlForSecurityDisplay().
     */
    default String getTopLevelOrigin() {
        return null;
    }

    /**
     * @return The scheme, host, and port of the last committed URL of the iframe that invoked the
     * PaymentRequest API as formatted by UrlFormatter.formatUrlForSecurityDisplay().
     */
    default String getPaymentRequestOrigin() {
        return null;
    }

    /**
     * @return The certificate chain of the top-level context as returned by
     * CertificateChainHelper.getCertificateChain(). Can be null.
     */
    @Nullable
    default byte[][] getCertificateChain() {
        return null;
    }

    /**
     * @return The unmodifiable mapping of method names to modifiers, which include modified totals
     * and additional line items. Used to display modified totals for each payment instrument,
     * modified total in order summary, and additional line items in order summary. Should not be
     * null.
     */
    default Map<String, PaymentDetailsModifier> getModifiers() {
        return null;
    }

    /**
     * @return Whether crawling the web for just-in-time installable payment handlers is enabled.
     */
    default boolean getMayCrawl() {
        return false;
    }

    /**
     * @return The listener for payment method, shipping address, and shipping option change events.
     */
    default PaymentRequestUpdateEventCallback getPaymentRequestUpdateEventCallback() {
        return null;
    }
}
