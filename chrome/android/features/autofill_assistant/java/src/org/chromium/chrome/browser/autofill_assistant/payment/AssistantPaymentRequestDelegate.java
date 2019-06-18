// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;

/** Delegate for the Payment Request UI. */
@JNINamespace("autofill_assistant")
public class AssistantPaymentRequestDelegate {
    private long mNativeAssistantPaymentRequestDelegate;

    @CalledByNative
    private static AssistantPaymentRequestDelegate create(
            long nativeAssistantPaymentRequestDelegate) {
        return new AssistantPaymentRequestDelegate(nativeAssistantPaymentRequestDelegate);
    }

    private AssistantPaymentRequestDelegate(long nativeAssistantPaymentRequestDelegate) {
        mNativeAssistantPaymentRequestDelegate = nativeAssistantPaymentRequestDelegate;
    }

    public void onContactInfoChanged(@Nullable AutofillContact contact) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            String name = null;
            String phone = null;
            String email = null;

            if (contact != null) {
                name = contact.getPayerName();
                phone = contact.getPayerPhone();
                email = contact.getPayerEmail();
            }

            nativeOnContactInfoChanged(mNativeAssistantPaymentRequestDelegate, name, phone, email);
        }
    }

    public void onShippingAddressChanged(@Nullable AutofillAddress address) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            nativeOnShippingAddressChanged(mNativeAssistantPaymentRequestDelegate,
                    address != null ? address.getProfile() : null);
        }
    }

    public void onPaymentMethodChanged(@Nullable AutofillPaymentInstrument paymentInstrument) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            nativeOnCreditCardChanged(mNativeAssistantPaymentRequestDelegate,
                    paymentInstrument != null ? paymentInstrument.getCard() : null);
        }
    }

    public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            nativeOnTermsAndConditionsChanged(mNativeAssistantPaymentRequestDelegate, state);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantPaymentRequestDelegate = 0;
    }

    private native void nativeOnContactInfoChanged(long nativeAssistantPaymentRequestDelegate,
            @Nullable String payerName, @Nullable String payerPhone, @Nullable String payerEmail);
    private native void nativeOnShippingAddressChanged(long nativeAssistantPaymentRequestDelegate,
            @Nullable PersonalDataManager.AutofillProfile address);
    private native void nativeOnCreditCardChanged(long nativeAssistantPaymentRequestDelegate,
            @Nullable PersonalDataManager.CreditCard card);
    private native void nativeOnTermsAndConditionsChanged(
            long nativeAssistantPaymentRequestDelegate, int state);
}
