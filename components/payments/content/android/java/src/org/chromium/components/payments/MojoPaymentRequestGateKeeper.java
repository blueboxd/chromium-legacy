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

/**
 * Guards against invalid mojo parameters and enforces correct call sequence from mojo IPC in the
 * untrusted renderer, so ComponentPaymentRequestImpl does not have to.
 */
/* package */ class MojoPaymentRequestGateKeeper implements PaymentRequest {
    private final ComponentPaymentRequestImplCreator mComponentPaymentRequestImplCreator;
    private ComponentPaymentRequestImpl mComponentPaymentRequestImpl;

    /** The creator of ComponentPaymentRequestImpl. */
    /* package */ interface ComponentPaymentRequestImplCreator {
        /**
         * Create an instance of ComponentPaymentRequestImpl if the parameters are valid.
         * @param client The client of the renderer PaymentRequest, need validation before usage.
         * @param methodData The supported methods specified by the merchant, need validation before
         *         usage.
         * @param details The payment details specified by the merchant, need validation before
         *         usage.
         * @param options The payment options specified by the merchant, need validation before
         *         usage.
         * @param googlePayBridgeEligible True when the renderer process deems the current request
         *         eligible for the skip-to-GPay experimental flow. It is ultimately up to the
         * browser process to determine whether to trigger it.
         * @param onClosedListener The listener that's invoked when ComponentPaymentRequestImpl has
         *         just closed.
         * @return The created instance, if the parameters are valid; otherwise, null.
         */
        ComponentPaymentRequestImpl createComponentPaymentRequestImplIfParamsValid(
                PaymentRequestClient client, PaymentMethodData[] methodData, PaymentDetails details,
                PaymentOptions options, boolean googlePayBridgeEligible, Runnable onClosedListener);
    }

    /**
     * Create an instance of MojoPaymentRequestGateKeeper.
     * @param creator The creator of ComponentPaymentRequestImpl.
     */
    /* package */ MojoPaymentRequestGateKeeper(ComponentPaymentRequestImplCreator creator) {
        mComponentPaymentRequestImplCreator = creator;
    }

    // Implement PaymentRequest:
    @Override
    public void init(PaymentRequestClient client, PaymentMethodData[] methodData,
            PaymentDetails details, PaymentOptions options, boolean googlePayBridgeEligible) {
        if (mComponentPaymentRequestImpl != null) {
            mComponentPaymentRequestImpl.abortForInvalidDataFromRenderer(
                    ErrorStrings.ATTEMPTED_INITIALIZATION_TWICE);
            mComponentPaymentRequestImpl = null;
            return;
        }

        // Note that a null value would be assigned when the params are invalid.
        mComponentPaymentRequestImpl =
                mComponentPaymentRequestImplCreator.createComponentPaymentRequestImplIfParamsValid(
                        client, methodData, details, options, googlePayBridgeEligible,
                        this::onComponentPaymentRequestImplClosed);
    }

    private void onComponentPaymentRequestImplClosed() {
        mComponentPaymentRequestImpl = null;
    }

    // Implement PaymentRequest:
    @Override
    public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.show(isUserGesture, waitForUpdatedDetails);
    }

    // Implement PaymentRequest:
    @Override
    public void updateWith(PaymentDetails details) {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.updateWith(details);
    }

    // Implement PaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated() {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.onPaymentDetailsNotUpdated();
    }

    // Implement PaymentRequest:
    @Override
    public void abort() {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.abort();
    }

    // Implement PaymentRequest:
    @Override
    public void complete(int result) {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.complete(result);
    }

    // Implement PaymentRequest:
    @Override
    public void retry(PaymentValidationErrors errors) {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.retry(errors);
    }

    // Implement PaymentRequest:
    @Override
    public void canMakePayment() {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.canMakePayment();
    }

    // Implement PaymentRequest:
    @Override
    public void hasEnrolledInstrument() {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.hasEnrolledInstrument();
    }

    // Implement PaymentRequest:
    @Override
    public void close() {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.closeByRenderer();
        mComponentPaymentRequestImpl = null;
    }

    // Implement PaymentRequest:
    @Override
    public void onConnectionError(MojoException e) {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.onConnectionError(e);
        mComponentPaymentRequestImpl = null;
    }
}
