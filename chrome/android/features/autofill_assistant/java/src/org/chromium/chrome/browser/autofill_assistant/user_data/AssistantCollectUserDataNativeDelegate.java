// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;

/** Delegate for the Collect user data UI which forwards events to a native counterpart. */
@JNINamespace("autofill_assistant")
public class AssistantCollectUserDataNativeDelegate implements AssistantCollectUserDataDelegate {
    private long mNativeAssistantCollectUserDataDelegate;

    @CalledByNative
    private static AssistantCollectUserDataNativeDelegate create(
            long nativeAssistantCollectUserDataDelegate) {
        return new AssistantCollectUserDataNativeDelegate(nativeAssistantCollectUserDataDelegate);
    }

    private AssistantCollectUserDataNativeDelegate(long nativeAssistantCollectUserDataDelegate) {
        mNativeAssistantCollectUserDataDelegate = nativeAssistantCollectUserDataDelegate;
    }

    @Override
    public void onContactInfoChanged(@Nullable AutofillContact contact) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            String name = null;
            String phone = null;
            String email = null;

            if (contact != null) {
                name = contact.getPayerName();
                phone = contact.getPayerPhone();
                email = contact.getPayerEmail();
            }

            nativeOnContactInfoChanged(mNativeAssistantCollectUserDataDelegate, name, phone, email);
        }
    }

    @Override
    public void onShippingAddressChanged(@Nullable AutofillAddress address) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            nativeOnShippingAddressChanged(mNativeAssistantCollectUserDataDelegate,
                    address != null ? address.getProfile() : null);
        }
    }

    @Override
    public void onPaymentMethodChanged(@Nullable AutofillPaymentInstrument paymentInstrument) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            nativeOnCreditCardChanged(mNativeAssistantCollectUserDataDelegate,
                    paymentInstrument != null ? paymentInstrument.getCard() : null);
        }
    }

    @Override
    public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            nativeOnTermsAndConditionsChanged(mNativeAssistantCollectUserDataDelegate, state);
        }
    }

    @Override
    public void onTermsAndConditionsLinkClicked(int link) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            nativeOnTermsAndConditionsLinkClicked(mNativeAssistantCollectUserDataDelegate, link);
        }
    }

    @Override
    public void onLoginChoiceChanged(AssistantLoginChoice loginChoice) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            nativeOnLoginChoiceChanged(mNativeAssistantCollectUserDataDelegate,
                    loginChoice != null ? loginChoice.getIdentifier() : null);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantCollectUserDataDelegate = 0;
    }

    private native void nativeOnContactInfoChanged(long nativeAssistantCollectUserDataDelegate,
            @Nullable String payerName, @Nullable String payerPhone, @Nullable String payerEmail);
    private native void nativeOnShippingAddressChanged(long nativeAssistantCollectUserDataDelegate,
            @Nullable PersonalDataManager.AutofillProfile address);
    private native void nativeOnCreditCardChanged(long nativeAssistantCollectUserDataDelegate,
            @Nullable PersonalDataManager.CreditCard card);
    private native void nativeOnTermsAndConditionsChanged(
            long nativeAssistantCollectUserDataDelegate, int state);
    private native void nativeOnTermsAndConditionsLinkClicked(
            long nativeAssistantCollectUserDataDelegate, int link);
    private native void nativeOnLoginChoiceChanged(
            long nativeAssistantCollectUserDataDelegate, String choice);
}
