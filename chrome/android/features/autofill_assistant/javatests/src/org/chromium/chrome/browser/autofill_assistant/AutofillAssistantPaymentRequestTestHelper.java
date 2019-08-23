// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator.DIVIDER_TAG;

import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestDelegate;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantTermsAndConditionsState;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantVerticalExpander;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantVerticalExpanderAccordion;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;
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
        final List<View> mDividers;

        ViewHolder(AssistantPaymentRequestCoordinator coordinator) {
            mAccordion = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_ACCORDION_TAG);
            mContactSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_CONTACT_DETAILS_SECTION_TAG);
            mPaymentSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_TAG);
            mShippingSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.PAYMENT_REQUEST_SHIPPING_ADDRESS_SECTION_TAG);
            mDividers = findViewsWithTag(coordinator.getView(), DIVIDER_TAG);
        }

        /** Returns all views with a matching tag. */
        private List<View> findViewsWithTag(View view, Object tag) {
            List<View> viewsWithTag = new ArrayList<>();
            if (view instanceof ViewGroup) {
                for (int i = 0; i < ((ViewGroup) view).getChildCount(); i++) {
                    viewsWithTag.addAll(findViewsWithTag(((ViewGroup) view).getChildAt(i), tag));
                }
            }
            if (view.getTag() != null && view.getTag().equals(tag)) {
                viewsWithTag.add(view);
            }
            return viewsWithTag;
        }
    }

    /**
     * Simple mock delegate which stores the currently selected PR items.
     *  TODO(crbug.com/860868): Remove this once PR is fully a MVC component, in which case one
     *  should be able to get the currently selected items by asking the model.
     */
    static class MockDelegate implements AssistantPaymentRequestDelegate {
        AutofillContact mContact;
        AutofillAddress mAddress;
        AutofillPaymentInstrument mPaymentMethod;
        @AssistantTermsAndConditionsState
        int mTermsStatus;
        @Nullable
        Integer mLastLinkClicked;

        @Override
        public void onContactInfoChanged(@Nullable AutofillContact contact) {
            mContact = contact;
        }

        @Override
        public void onShippingAddressChanged(@Nullable AutofillAddress address) {
            mAddress = address;
        }

        @Override
        public void onPaymentMethodChanged(@Nullable AutofillPaymentInstrument paymentInstrument) {
            mPaymentMethod = paymentInstrument;
        }

        @Override
        public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
            mTermsStatus = state;
        }

        @Override
        public void onTermsAndConditionsLinkClicked(int link) {
            mLastLinkClicked = link;
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

    /**
     * Adds a new profile with dummy data to the personal data manager.
     *
     * @param fullName The full name for the profile to create.
     * @param email The email for the profile to create.
     * @param postcode The postcode of the billing address.
     * @return the GUID of the created profile.
     */
    public String addDummyProfile(String fullName, String email, String postcode)
            throws TimeoutException, InterruptedException {
        PersonalDataManager.AutofillProfile profile = new PersonalDataManager.AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */, fullName, "Acme Inc.",
                "123 Main", "California", "Los Angeles", "", postcode, "", "Uzbekistan",
                "555 123-4567", email, "");
        return setProfile(profile);
    }

    public String addDummyProfile(String fullName, String email)
            throws TimeoutException, InterruptedException {
        return addDummyProfile(fullName, email, "90210");
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

    /**
     * Adds a credit card with dummy data to the personal data manager.
     *
     * @param billingAddressId The billing address profile GUID.
     * @return the GUID of the created credit card
     */
    public String addDummyCreditCard(String billingAddressId)
            throws TimeoutException, InterruptedException {
        String profileName = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getProfile(billingAddressId).getFullName());

        PersonalDataManager.CreditCard creditCard = new PersonalDataManager.CreditCard("",
                "https://example.com", true, true, profileName, "4111111111111111", "1111", "12",
                "2050", "visa", org.chromium.chrome.autofill_assistant.R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */);
        return setCreditCard(creditCard);
    }

    public void deleteProfile(final String guid) throws InterruptedException, TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().deleteProfile(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    public void deleteCreditCard(final String guid) throws InterruptedException, TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().deleteCreditCard(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
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
