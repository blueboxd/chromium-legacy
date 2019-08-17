// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;

/** Delegate for the Payment Request UI which forwards events to a native counterpart. */
@JNINamespace("autofill_assistant")
public class AssistantPaymentRequestNativeDelegate implements AssistantPaymentRequestDelegate {
    private long mNativeAssistantPaymentRequestDelegate;

    @CalledByNative
    private static AssistantPaymentRequestNativeDelegate create(
            long nativeAssistantPaymentRequestDelegate) {
        return new AssistantPaymentRequestNativeDelegate(nativeAssistantPaymentRequestDelegate);
    }

    private AssistantPaymentRequestNativeDelegate(long nativeAssistantPaymentRequestDelegate) {
        mNativeAssistantPaymentRequestDelegate = nativeAssistantPaymentRequestDelegate;
    }

    @Override
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

            AssistantPaymentRequestNativeDelegateJni.get().onContactInfoChanged(
                    mNativeAssistantPaymentRequestDelegate,
                    AssistantPaymentRequestNativeDelegate.this, name, phone, email);
        }
    }

    @Override
    public void onShippingAddressChanged(@Nullable AutofillAddress address) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            AssistantPaymentRequestNativeDelegateJni.get().onShippingAddressChanged(
                    mNativeAssistantPaymentRequestDelegate,
                    AssistantPaymentRequestNativeDelegate.this,
                    address != null ? address.getProfile() : null);
        }
    }

    @Override
    public void onPaymentMethodChanged(@Nullable AutofillPaymentInstrument paymentInstrument) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            AssistantPaymentRequestNativeDelegateJni.get().onCreditCardChanged(
                    mNativeAssistantPaymentRequestDelegate,
                    AssistantPaymentRequestNativeDelegate.this,
                    paymentInstrument != null ? paymentInstrument.getCard() : null);
        }
    }

    @Override
    public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            AssistantPaymentRequestNativeDelegateJni.get().onTermsAndConditionsChanged(
                    mNativeAssistantPaymentRequestDelegate,
                    AssistantPaymentRequestNativeDelegate.this, state);
        }
    }

    @Override
    public void onTermsAndConditionsLinkClicked(int link) {
        if (mNativeAssistantPaymentRequestDelegate != 0) {
            AssistantPaymentRequestNativeDelegateJni.get().onTermsAndConditionsLinkClicked(
                    mNativeAssistantPaymentRequestDelegate,
                    AssistantPaymentRequestNativeDelegate.this, link);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantPaymentRequestDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onContactInfoChanged(long nativeAssistantPaymentRequestDelegate,
                AssistantPaymentRequestNativeDelegate caller, @Nullable String payerName,
                @Nullable String payerPhone, @Nullable String payerEmail);
        void onShippingAddressChanged(long nativeAssistantPaymentRequestDelegate,
                AssistantPaymentRequestNativeDelegate caller,
                @Nullable PersonalDataManager.AutofillProfile address);
        void onCreditCardChanged(long nativeAssistantPaymentRequestDelegate,
                AssistantPaymentRequestNativeDelegate caller,
                @Nullable PersonalDataManager.CreditCard card);
        void onTermsAndConditionsChanged(long nativeAssistantPaymentRequestDelegate,
                AssistantPaymentRequestNativeDelegate caller, int state);
        void onTermsAndConditionsLinkClicked(long nativeAssistantPaymentRequestDelegate,
                AssistantPaymentRequestNativeDelegate caller, int link);
    }
}
