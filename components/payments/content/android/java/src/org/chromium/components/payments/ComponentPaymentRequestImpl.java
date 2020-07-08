// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.lang.ref.WeakReference;

/**
 * Android implementation of the PaymentRequest service defined in
 * third_party/blink/public/mojom/payments/payment_request.mojom. This component provides the parts
 * shareable between Clank and WebLayer. The Clank specific logic lives in
 * org.chromium.chrome.browser.payments.PaymentRequestImpl.
 */
public class ComponentPaymentRequestImpl implements PaymentRequest {
    private final ComponentPaymentRequestDelegate mDelegate;

    /**
     * The delegate of {@link ComponentPaymentRequestImpl}.
     */
    public interface ComponentPaymentRequestDelegate {
        // The implementation of the same methods in {@link PaymentRequest).
        void init(PaymentRequestClient client, PaymentMethodData[] methodData,
                PaymentDetails details, PaymentOptions options, boolean googlePayBridgeEligible);
        void show(boolean isUserGesture, boolean waitForUpdatedDetails);
        void updateWith(PaymentDetails details);
        void onPaymentDetailsNotUpdated();
        void abort();
        void complete(int result);
        void retry(PaymentValidationErrors errors);
        void hasEnrolledInstrument(boolean perMethodQuota);
        void canMakePayment();
        void close();
        void onConnectionError(MojoException e);

        /**
         * Set a weak reference to the client of this delegate.
         * @param componentsPaymentRequestImpl A weak reference to the client of this delegate.
         */
        void setComponentPaymentRequestImpl(
                WeakReference<ComponentPaymentRequestImpl> componentsPaymentRequestImpl);
    }

    /**
     * Build an instance of the PaymentRequest implementation.
     * @param delegate A delegate of the instance.
     */
    public ComponentPaymentRequestImpl(ComponentPaymentRequestDelegate delegate) {
        mDelegate = delegate;
        mDelegate.setComponentPaymentRequestImpl(new WeakReference<>(this));
    }

    @Override
    public void init(PaymentRequestClient client, PaymentMethodData[] methodData,
            PaymentDetails details, PaymentOptions options, boolean googlePayBridgeEligible) {
        mDelegate.init(client, methodData, details, options, googlePayBridgeEligible);
    }

    @Override
    public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        mDelegate.show(isUserGesture, waitForUpdatedDetails);
    }

    @Override
    public void updateWith(PaymentDetails details) {
        mDelegate.updateWith(details);
    }

    @Override
    public void onPaymentDetailsNotUpdated() {
        mDelegate.onPaymentDetailsNotUpdated();
    }

    @Override
    public void abort() {
        mDelegate.abort();
    }

    @Override
    public void complete(int result) {
        mDelegate.complete(result);
    }

    @Override
    public void retry(PaymentValidationErrors errors) {
        mDelegate.retry(errors);
    }

    @Override
    public void canMakePayment() {
        mDelegate.canMakePayment();
    }

    @Override
    public void hasEnrolledInstrument(boolean perMethodQuota) {
        mDelegate.hasEnrolledInstrument(perMethodQuota);
    }

    @Override
    public void close() {
        mDelegate.close();
    }

    @Override
    public void onConnectionError(MojoException e) {
        mDelegate.onConnectionError(e);
    }
}
