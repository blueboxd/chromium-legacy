// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.hamcrest.Matchers.containsString;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.support.design.widget.CoordinatorLayout;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantChoiceList;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestModel;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantTermsAndConditionsState;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the Autofill Assistant payment request UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillAssistantPaymentRequestUiTest {
    private AutofillAssistantPaymentRequestTestHelper mHelper;

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
        mHelper = new AutofillAssistantPaymentRequestTestHelper();
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantPaymentRequestCoordinator createPaymentRequestCoordinator(
            AssistantPaymentRequestModel model) {
        ThreadUtils.assertOnUiThread();
        AssistantPaymentRequestCoordinator coordinator =
                new AssistantPaymentRequestCoordinator(getActivity(), model);

        CoordinatorLayout.LayoutParams lp = new CoordinatorLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.BOTTOM;

        ViewGroup chromeCoordinatorView = getActivity().findViewById(R.id.coordinator);
        chromeCoordinatorView.addView(coordinator.getView(), lp);

        return coordinator;
    }

    /**
     * Test assumptions about the initial state of the payment request.
     */
    @Test
    @MediumTest
    public void testInitialState() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
            AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);

            assertFalse(model.get(AssistantPaymentRequestModel.VISIBLE));
            assertFalse("PR is initially invisible", coordinator.getView().isShown());
            assertNull("Initially, the model should not contain any profiles",
                    model.get(AssistantPaymentRequestModel.AVAILABLE_PROFILES));
            assertNull("Initially, the model should not contain any payment methods",
                    model.get(AssistantPaymentRequestModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS));
            assertNull("Initially, no payment method is supported",
                    model.get(AssistantPaymentRequestModel.SUPPORTED_PAYMENT_METHODS));
            assertNull("Initially, no basic card network filter is set",
                    model.get(AssistantPaymentRequestModel.SUPPORTED_BASIC_CARD_NETWORKS));
            assertNull("Initially, no section is expanded",
                    model.get(AssistantPaymentRequestModel.EXPANDED_SECTION));
            assertNull(model.get(AssistantPaymentRequestModel.DELEGATE));
            assertNull(model.get(AssistantPaymentRequestModel.WEB_CONTENTS));
            assertNull(model.get(AssistantPaymentRequestModel.SHIPPING_ADDRESS));
            assertNull(model.get(AssistantPaymentRequestModel.PAYMENT_METHOD));
            assertNull(model.get(AssistantPaymentRequestModel.CONTACT_DETAILS));
            assertEquals(AssistantTermsAndConditionsState.NOT_SELECTED,
                    model.get(AssistantPaymentRequestModel.TERMS_STATUS));

            /** Test initial UI state. */
            AutofillAssistantPaymentRequestTestHelper.ViewHolder viewHolder =
                    new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator);
            assertFalse(viewHolder.mContactSection.isExpanded());
            assertFalse(viewHolder.mPaymentSection.isExpanded());
            assertFalse(viewHolder.mShippingSection.isExpanded());
        });
    }

    /**
     * Sections become visible/invisible depending on model changes.
     */
    @Test
    @MediumTest
    public void testSectionVisibility() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
            AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);

            AutofillAssistantPaymentRequestTestHelper.ViewHolder viewHolder =
                    new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator);

            /** Initially, everything is invisible. */
            assertFalse(coordinator.getView().isShown());

            /** PR is visible, but no section was requested: all sections should be invisible. */
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
            assertTrue(coordinator.getView().isShown());
            assertFalse(viewHolder.mContactSection.isShown());
            assertFalse(viewHolder.mPaymentSection.isShown());
            assertFalse(viewHolder.mShippingSection.isShown());

            /** Contact details should be visible if either name, phone, or email is requested. */
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            assertTrue(viewHolder.mContactSection.isShown());
            assertFalse(viewHolder.mPaymentSection.isShown());
            assertFalse(viewHolder.mShippingSection.isShown());

            model.set(AssistantPaymentRequestModel.REQUEST_NAME, false);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            assertTrue(viewHolder.mContactSection.isShown());
            assertFalse(viewHolder.mPaymentSection.isShown());
            assertFalse(viewHolder.mShippingSection.isShown());

            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, false);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            assertTrue(viewHolder.mContactSection.isShown());
            assertFalse(viewHolder.mPaymentSection.isShown());
            assertFalse(viewHolder.mShippingSection.isShown());

            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            assertTrue(viewHolder.mContactSection.isShown());
            assertFalse(viewHolder.mPaymentSection.isShown());
            assertFalse(viewHolder.mShippingSection.isShown());

            /** Payment method section visibility test. */
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            assertTrue(viewHolder.mPaymentSection.isShown());

            /** Shipping address visibility test. */
            model.set(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS, true);
            assertTrue(viewHolder.mShippingSection.isShown());
        });
    }

    /**
     * Test assumptions about a payment request for a case where the personal data manager does not
     * contain any profiles or payment methods, i.e., all PR sections should be empty.
     */
    @Test
    @MediumTest
    public void testEmptyPaymentRequest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
            AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);

            /** Request all PR sections. */
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            model.set(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);

            AutofillAssistantPaymentRequestTestHelper.ViewHolder viewHolder =
                    new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator);

            /** Empty sections should display the 'add' button in their title. */
            assertTrue(viewHolder.mContactSection.findViewById(R.id.section_title_add_button)
                               .isShown());
            assertTrue(viewHolder.mPaymentSection.findViewById(R.id.section_title_add_button)
                               .isShown());
            assertTrue(viewHolder.mShippingSection.findViewById(R.id.section_title_add_button)
                               .isShown());

            /** Empty sections should be 'fixed', i.e., they can not be expanded. */
            assertTrue(viewHolder.mContactSection.isFixed());
            assertTrue(viewHolder.mPaymentSection.isFixed());
            assertTrue(viewHolder.mShippingSection.isFixed());

            /** Empty sections are collapsed. */
            assertFalse(viewHolder.mContactSection.isExpanded());
            assertFalse(viewHolder.mPaymentSection.isExpanded());
            assertFalse(viewHolder.mShippingSection.isExpanded());

            /** Empty sections should be empty. */
            AssistantChoiceList contactsList =
                    viewHolder.mContactSection.findViewById(R.id.section_choice_list);
            AssistantChoiceList paymentsList =
                    viewHolder.mPaymentSection.findViewById(R.id.section_choice_list);
            AssistantChoiceList shippingList =
                    viewHolder.mShippingSection.findViewById(R.id.section_choice_list);
            assertEquals(0, contactsList.getItemCount());
            assertEquals(0, paymentsList.getItemCount());
            assertEquals(0, shippingList.getItemCount());
        });
    }

    /**
     * Test assumptions about a payment request for a personal data manager with a complete profile
     * and payment method, i.e., all PR sections should be non-empty.
     */
    @Test
    @MediumTest
    public void testNonEmptyPaymentRequest() throws TimeoutException, InterruptedException {
        /**
         * Add complete profile and credit card to the personal data manager. Must be done outside
         * UI thread!
         */
        PersonalDataManager.AutofillProfile profile = new PersonalDataManager.AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */, "Maggie Simpson",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "Uzbekistan",
                "555 123-4567", "maggie@simpson.com", "");
        String billingAddressId = mHelper.setProfile(profile);
        PersonalDataManager.CreditCard creditCard = new PersonalDataManager.CreditCard("",
                "https://example.com", true, true, "Jon Doe", "4111111111111111", "1111", "12",
                "2050", "visa", org.chromium.chrome.R.drawable.visa_card, CardType.UNKNOWN,
                billingAddressId, "" /* serverId */);
        mHelper.setCreditCard(creditCard);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
            AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);

            /** Request all PR sections. */
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            model.set(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);

            AutofillAssistantPaymentRequestTestHelper.ViewHolder viewHolder =
                    new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator);

            /** Non-empty sections should not display the 'add' button in their title. */
            assertFalse(viewHolder.mContactSection.findViewById(R.id.section_title_add_button)
                                .isShown());
            assertFalse(viewHolder.mPaymentSection.findViewById(R.id.section_title_add_button)
                                .isShown());
            assertFalse(viewHolder.mShippingSection.findViewById(R.id.section_title_add_button)
                                .isShown());

            /** Non-empty sections should not be 'fixed', i.e., they can be expanded. */
            assertFalse(viewHolder.mContactSection.isFixed());
            assertFalse(viewHolder.mPaymentSection.isFixed());
            assertFalse(viewHolder.mShippingSection.isFixed());

            /** Check contents of sections. */
            AssistantChoiceList contactsList =
                    viewHolder.mContactSection.findViewById(R.id.section_choice_list);
            AssistantChoiceList paymentsList =
                    viewHolder.mPaymentSection.findViewById(R.id.section_choice_list);
            AssistantChoiceList shippingList =
                    viewHolder.mShippingSection.findViewById(R.id.section_choice_list);
            assertEquals(1, contactsList.getItemCount());
            assertEquals(1, paymentsList.getItemCount());
            assertEquals(1, shippingList.getItemCount());
            testContact("maggie@simpson.com", "Maggie Simpson\nmaggie@simpson.com",
                    viewHolder.mContactSection.getCollapsedView(), contactsList.getItem(0));
            testPaymentMethod("1111", "Jon Doe", "12/2050",
                    viewHolder.mPaymentSection.getCollapsedView(), paymentsList.getItem(0));
            testShippingAddress("Maggie Simpson",
                    "Acme Inc., 123 Main, 90210 Los Angeles, California",
                    "Acme Inc., 123 Main, 90210 Los Angeles, California, Uzbekistan",
                    viewHolder.mShippingSection.getCollapsedView(), shippingList.getItem(0));
        });
    }

    private void testContact(String expectedContactSummary, String expectedContactFullDescription,
            View summaryView, View fullView) {
        TextView contactSummary = summaryView.findViewById(R.id.contact_summary);
        assertEquals(expectedContactSummary, contactSummary.getText());
        assertFalse(summaryView.findViewById(R.id.incomplete_error).isShown());

        TextView contactFull = fullView.findViewById(R.id.contact_full);
        assertEquals(expectedContactFullDescription, contactFull.getText());
        assertFalse(fullView.findViewById(R.id.incomplete_error).isShown());
    }

    private void testPaymentMethod(String expectedObfuscatedCardNumber, String expectedCardName,
            String expectedCardExpiration, View summaryView, View fullView) {
        TextView paymentMethodNumber = summaryView.findViewById(R.id.credit_card_number);
        testCreditCardNumber(expectedObfuscatedCardNumber, paymentMethodNumber);
        TextView paymentMethodExpirationDate =
                summaryView.findViewById(R.id.credit_card_expiration);
        assertEquals(expectedCardExpiration, paymentMethodExpirationDate.getText());
        assertNull(summaryView.findViewById(R.id.credit_card_name));
        assertFalse(summaryView.findViewById(R.id.incomplete_error).isShown());

        paymentMethodNumber = fullView.findViewById(R.id.credit_card_number);
        testCreditCardNumber(expectedObfuscatedCardNumber, paymentMethodNumber);
        paymentMethodExpirationDate = fullView.findViewById(R.id.credit_card_expiration);
        assertEquals(expectedCardExpiration, paymentMethodExpirationDate.getText());
        TextView paymentMethodCardName = fullView.findViewById(R.id.credit_card_name);
        assertEquals(expectedCardName, paymentMethodCardName.getText());
        assertFalse(fullView.findViewById(R.id.incomplete_error).isShown());
    }

    private void testCreditCardNumber(String expectedObfuscatedNumber, TextView actual) {
        assertThat(actual.getText().toString(), containsString(expectedObfuscatedNumber));
    }

    private void testShippingAddress(String expectedFullName, String expectedShortAddress,
            String expectedFullAddress, View summaryView, View fullView) {
        TextView addressName = summaryView.findViewById(R.id.full_name);
        assertEquals(expectedFullName, addressName.getText());
        TextView addressShort = summaryView.findViewById(R.id.short_address);
        assertEquals(expectedShortAddress, addressShort.getText());
        assertFalse(summaryView.findViewById(R.id.incomplete_error).isShown());

        addressName = fullView.findViewById(R.id.full_name);
        assertEquals(expectedFullName, addressName.getText());
        TextView addressFull = fullView.findViewById(R.id.full_address);
        assertEquals(expectedFullAddress, addressFull.getText());
        assertFalse(fullView.findViewById(R.id.incomplete_error).isShown());
    }
}
