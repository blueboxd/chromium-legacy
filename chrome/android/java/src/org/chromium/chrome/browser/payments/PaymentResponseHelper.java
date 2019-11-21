// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.NormalizedAddressRequestDelegate;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.widget.prefeditor.EditableOption;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentResponse;

/**
 * The helper class to create and prepare a PaymentResponse.
 */
public class PaymentResponseHelper implements NormalizedAddressRequestDelegate {
    /**
     * Observer to be notified when the payment response is completed.
     */
    public interface PaymentResponseRequesterDelegate {
        /*
         * Called when the payment response is ready to be sent to the merchant.
         *
         * @param response The payment response to send to the merchant.
         */
        void onPaymentResponseReady(PaymentResponse response);
    }

    private AutofillAddress mSelectedShippingAddress;
    private final AutofillContact mSelectedContact;
    private PaymentResponse mPaymentResponse;
    private PaymentResponseRequesterDelegate mDelegate;
    private boolean mIsWaitingForShippingNormalization;
    private boolean mIsWaitingForPaymentsDetails = true;
    private final PaymentInstrument mSelectedPaymentInstrument;
    private final PaymentOptions mPaymentOptions;
    private final boolean mSkipToGpay;
    private PayerData mPayerDataFromInstrument;

    /**
     * Builds a helper to contruct and fill a PaymentResponse.
     *
     * @param selectedShippingAddress   The shipping address picked by the user.
     * @param selectedShippingOption    The shipping option picked by the user.
     * @param selectedContact           The contact info picked by the user.
     * @param selectedPaymentInstrument The payment instrument picked by the user.
     * @param paymentOptions            The paymentOptions of the corresponding payment request.
     * @param skipToGpay                Whether or not Gpay bridge is activated for skip to Gpay.
     * @param delegate                  The object that will receive the completed PaymentResponse.
     */
    public PaymentResponseHelper(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, EditableOption selectedContact,
            PaymentInstrument selectedPaymentInstrument, PaymentOptions paymentOptions,
            boolean skipToGpay, PaymentResponseRequesterDelegate delegate) {
        mPaymentResponse = new PaymentResponse();
        mPaymentResponse.payer = new PayerDetail();

        mDelegate = delegate;
        mSelectedPaymentInstrument = selectedPaymentInstrument;
        mPaymentOptions = paymentOptions;
        mSkipToGpay = skipToGpay;

        // Contacts are created in PaymentRequestImpl.init(). These should all be instances of
        // AutofillContact.
        mSelectedContact = (AutofillContact) selectedContact;

        // Set up the shipping option section of the response when it comes from payment sheet
        // (Shipping option comes from payment app when the app can handle shipping, or from Gpay
        // when skipToGpay is true).
        if (mPaymentOptions.requestShipping && !mSelectedPaymentInstrument.handlesShippingAddress()
                && !mSkipToGpay) {
            assert selectedShippingOption != null;
            assert selectedShippingOption.getIdentifier() != null;
            mPaymentResponse.shippingOption = selectedShippingOption.getIdentifier();
        }

        // Set up the shipping address section of the response when it comes from payment sheet
        // (Shipping address comes from payment app when the app can handle shipping, or from Gpay
        // when skipToGpay is true).
        if (mPaymentOptions.requestShipping && !mSelectedPaymentInstrument.handlesShippingAddress()
                && !mSkipToGpay) {
            assert selectedShippingAddress != null;
            // Shipping addresses are created in PaymentRequestImpl.init(). These should all be
            // instances of AutofillAddress.
            mSelectedShippingAddress = (AutofillAddress) selectedShippingAddress;

            // Addresses to be sent to the merchant should always be complete.
            assert mSelectedShippingAddress.isComplete();

            // Record the use of the profile.
            PersonalDataManager.getInstance().recordAndLogProfileUse(
                    mSelectedShippingAddress.getProfile().getGUID());

            mPaymentResponse.shippingAddress = mSelectedShippingAddress.toPaymentAddress();

            // The shipping address needs to be normalized before sending the response to the
            // merchant.
            mIsWaitingForShippingNormalization = true;
            PersonalDataManager.getInstance().normalizeAddress(
                    mSelectedShippingAddress.getProfile(), this);
        }
    }

    /**
     * Called after the payment instrument's details were received.
     *
     * @param methodName          The method name of the payment instrument.
     * @param stringifiedDetails  A string containing all the details of the payment instrument's
     *                            details.
     * @param payerData           The payer data received from the payment app.
     */
    public void onInstrumentDetailsReceived(
            String methodName, String stringifiedDetails, PayerData payerData) {
        mPaymentResponse.methodName = methodName;
        mPaymentResponse.stringifiedDetails = stringifiedDetails;
        mPayerDataFromInstrument = payerData;

        mIsWaitingForPaymentsDetails = false;

        // Wait for the shipping address normalization before sending the response.
        if (!mIsWaitingForShippingNormalization) generatePaymentResponse();
    }

    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        // Check if a normalization is still required.
        if (!mIsWaitingForShippingNormalization) return;
        mIsWaitingForShippingNormalization = false;

        if (profile != null) {
            // The normalization finished first: use the normalized address.
            mSelectedShippingAddress.completeAddress(profile);
            mPaymentResponse.shippingAddress = mSelectedShippingAddress.toPaymentAddress();
        }

        // Wait for the payment details before sending the response.
        if (!mIsWaitingForPaymentsDetails) generatePaymentResponse();
    }

    @Override
    public void onCouldNotNormalize(AutofillProfile profile) {
        onAddressNormalized(profile);
    }

    private void generatePaymentResponse() {
        assert !mIsWaitingForPaymentsDetails;
        assert !mIsWaitingForShippingNormalization;

        // Set up the shipping section of the response when it comes from payment app.
        if (mPaymentOptions.requestShipping
                && mSelectedPaymentInstrument.handlesShippingAddress()) {
            mPaymentResponse.shippingAddress = mPayerDataFromInstrument.shippingAddress;
            mPaymentResponse.shippingOption = mPayerDataFromInstrument.selectedShippingOptionId;
        }

        // Set up the contact section of the response.
        if (mPaymentOptions.requestPayerName) {
            if (mSelectedPaymentInstrument.handlesPayerName()) {
                mPaymentResponse.payer.name = mPayerDataFromInstrument.payerName;
            } else if (!mSkipToGpay) {
                assert mSelectedContact != null;
                mPaymentResponse.payer.name = mSelectedContact.getPayerName();
            } else {
                // Gpay provides contact info when skip to Gpay is true.
            }
        }
        if (mPaymentOptions.requestPayerPhone) {
            if (mSelectedPaymentInstrument.handlesPayerPhone()) {
                mPaymentResponse.payer.phone = mPayerDataFromInstrument.payerPhone;
            } else if (!mSkipToGpay) {
                assert mSelectedContact != null;
                mPaymentResponse.payer.phone = mSelectedContact.getPayerPhone();
            } else {
                // Gpay provides contact info when skip to Gpay is true.
            }
        }
        if (mPaymentOptions.requestPayerEmail) {
            if (mSelectedPaymentInstrument.handlesPayerEmail()) {
                mPaymentResponse.payer.email = mPayerDataFromInstrument.payerEmail;
            } else if (!mSkipToGpay) {
                assert mSelectedContact != null;
                mPaymentResponse.payer.email = mSelectedContact.getPayerEmail();
            } else {
                // Gpay provides contact info when skip to Gpay is true.
            }
        }

        // Normalize the phone number only if it's not null since this calls native code.
        if (mPaymentResponse.payer.phone != null) {
            mPaymentResponse.payer.phone =
                    PhoneNumberUtil.formatForResponse(mPaymentResponse.payer.phone);
        }

        mDelegate.onPaymentResponseReady(mPaymentResponse);
    }
}
