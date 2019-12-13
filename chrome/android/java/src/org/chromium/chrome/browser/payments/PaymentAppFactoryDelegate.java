// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

/**
 * Interface for providing information to a payment app factory and receiving the list of payment
 * apps.
 */
public interface PaymentAppFactoryDelegate {
    /** @return The information that a factory needs to create payment apps. */
    PaymentAppFactoryParams getParams();

    /**
     * @param canMakePayment Whether a payment app can support requested payment method.
     */
    void onCanMakePaymentCalculated(boolean canMakePayment);

    /**
     * Called when a payment app factory has a resource identifier for text to be displayed to the
     * user. Only autofill has this.
     *
     * @param additionalTextResourceId The resource identifier for text to be displayed to the user.
     * Never 0, which is invalid.
     */
    void onAdditionalTextResourceId(int additionalTextResourceId);

    /**
     * Called when the autofill payment app factory has been created.
     *
     * @param factory The factory that creates autofill payment apps.
     */
    void onAutofillPaymentAppFactoryCreated(AutofillPaymentApp factory);

    /**
     * Called when a payment app factory has created a payment app.
     *
     * @param paymentApp A payment app.
     */
    void onPaymentAppCreated(PaymentInstrument paymentApp);

    /**
     * Called when a payment app factory has failed to create a payment app.
     *
     * @param errorMessage The error message for the web developer, e.g., "Failed to download the
     * web app manifest file."
     */
    void onPaymentAppCreationError(String errorMessage);

    /**
     * Called when the payment app factory has finished creating all payment apps.
     *
     * @param factory The factory that has finished creating all payment apps.
     */
    void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory);
}
