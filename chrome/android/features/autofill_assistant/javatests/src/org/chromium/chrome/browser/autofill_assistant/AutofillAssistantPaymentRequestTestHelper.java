// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantVerticalExpander;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantVerticalExpanderAccordion;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Helper class for testing autofill assistant payment request. Code adapted from
 * https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/autofill/AutofillTestHelper.java
 */
public class AutofillAssistantPaymentRequestTestHelper {
    private final CallbackHelper mOnPersonalDataChangedHelper = new CallbackHelper();

    /** Extracts the views from a coordinator. */
    static class ViewHolder {
        final AssistantVerticalExpanderAccordion mAccordion;
        final AssistantVerticalExpander mContactSection;
        final AssistantVerticalExpander mPaymentSection;
        final AssistantVerticalExpander mShippingSection;

        ViewHolder(AssistantPaymentRequestCoordinator coordinator) {
            mAccordion = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_ACCORDION_TAG);
            mContactSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_CONTACT_DETAILS_SECTION_TAG);
            mPaymentSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_TAG);
            mShippingSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_SHIPPING_ADDRESS_SECTION_TAG);
        }
    }

    public AutofillAssistantPaymentRequestTestHelper()
            throws TimeoutException, InterruptedException {
        registerDataObserver();
        setRequestTimeoutForTesting();
        setSyncServiceForTesting();
    }

    void setRequestTimeoutForTesting() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().setRequestTimeoutForTesting(0));
    }

    void setSyncServiceForTesting() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().setSyncServiceForTesting());
    }

    public String setProfile(final AutofillProfile profile)
            throws TimeoutException, InterruptedException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().setProfile(profile));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    public CreditCard getCreditCard(final String guid) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCreditCard(guid));
    }

    public String getShippingAddressLabelWithoutCountryForPaymentRequest(AutofillProfile profile) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> PersonalDataManager.getInstance()
                                   .getShippingAddressLabelWithoutCountryForPaymentRequest(
                                           profile));
    }

    public String getShippingAddressLabelWithCountryForPaymentRequest(AutofillProfile profile) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> PersonalDataManager.getInstance()
                                   .getShippingAddressLabelWithCountryForPaymentRequest(profile));
    }

    public String setCreditCard(final CreditCard card)
            throws TimeoutException, InterruptedException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().setCreditCard(card));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    private void registerDataObserver() throws TimeoutException, InterruptedException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        boolean isDataLoaded = TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> PersonalDataManager.getInstance().registerDataObserver(
                                () -> mOnPersonalDataChangedHelper.notifyCalled()));
        if (isDataLoaded) return;
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }
}
