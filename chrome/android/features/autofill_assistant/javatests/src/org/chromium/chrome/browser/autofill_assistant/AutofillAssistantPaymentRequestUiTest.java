// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withTagValue;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.VERTICAL_EXPANDER_CHEVRON;

import android.support.design.widget.CoordinatorLayout;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;

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
    public void testInitialState() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator =
                TestThreadUtils.runOnUiThreadBlocking(() -> createPaymentRequestCoordinator(model));

        /* Test initial model state. */
        assertThat(model.get(AssistantPaymentRequestModel.VISIBLE), is(false));
        assertThat(model.get(AssistantPaymentRequestModel.AVAILABLE_PROFILES), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS),
                nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.SUPPORTED_PAYMENT_METHODS), nullValue());
        assertThat(
                model.get(AssistantPaymentRequestModel.SUPPORTED_BASIC_CARD_NETWORKS), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.EXPANDED_SECTION), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.DELEGATE), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.WEB_CONTENTS), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.SHIPPING_ADDRESS), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.PAYMENT_METHOD), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.CONTACT_DETAILS), nullValue());
        assertThat(model.get(AssistantPaymentRequestModel.TERMS_STATUS),
                is(AssistantTermsAndConditionsState.NOT_SELECTED));

        /* Test initial UI state. */
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_choice_list),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_choice_list),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_choice_list),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        /* No section divider is visible. */
        for (View divider : viewHolder.mDividers) {
            onView(is(divider)).check(matches(not(isDisplayed())));
        }
    }

    /**
     * Sections become visible/invisible depending on model changes.
     */
    @Test
    @MediumTest
    public void testSectionVisibility() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator =
                TestThreadUtils.runOnUiThreadBlocking(() -> createPaymentRequestCoordinator(model));
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        /* Initially, everything is invisible. */
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));

        /* PR is visible, but no section was requested: all sections should be invisible. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantPaymentRequestModel.VISIBLE, true));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));
        onView(is(viewHolder.mContactSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        /* Contact details should be visible if either name, phone, or email is requested. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantPaymentRequestModel.REQUEST_NAME, true));
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, false);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
        });
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, false);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
        });
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
        });
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        /* Payment method section visibility test. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true));
        onView(is(viewHolder.mPaymentSection)).check(matches(isDisplayed()));

        /* Shipping address visibility test. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS, true));
        onView(is(viewHolder.mShippingSection)).check(matches(isDisplayed()));
    }

    /**
     * Test assumptions about a payment request for a case where the personal data manager does not
     * contain any profiles or payment methods, i.e., all PR sections should be empty.
     */
    @Test
    @MediumTest
    public void testEmptyPaymentRequest() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator =
                TestThreadUtils.runOnUiThreadBlocking(() -> createPaymentRequestCoordinator(model));
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        /* Request all PR sections. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            model.set(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
        });

        /* Empty sections should display the 'add' button in their title. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(isDisplayed()));

        /* Empty sections should be 'fixed', i.e., they can not be expanded. */
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        /* Empty sections are collapsed. */
        onView(allOf(withId(R.id.section_choice_list),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_choice_list),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_choice_list),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        /* Empty sections should be empty. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantChoiceList contactsList =
                    viewHolder.mContactSection.findViewById(R.id.section_choice_list);
            AssistantChoiceList paymentsList =
                    viewHolder.mPaymentSection.findViewById(R.id.section_choice_list);
            AssistantChoiceList shippingList =
                    viewHolder.mShippingSection.findViewById(R.id.section_choice_list);
            assertThat(contactsList.getItemCount(), is(0));
            assertThat(paymentsList.getItemCount(), is(0));
            assertThat(shippingList.getItemCount(), is(0));
        });
    }

    /**
     * Test assumptions about a payment request for a personal data manager with a complete profile
     * and payment method, i.e., all PR sections should be non-empty.
     */
    @Test
    @MediumTest
    public void testNonEmptyPaymentRequest() throws Exception {
        /* Add complete profile and credit card to the personal data manager. */
        PersonalDataManager.AutofillProfile profile = new PersonalDataManager.AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */, "Maggie Simpson",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "Uzbekistan",
                "555 123-4567", "maggie@simpson.com", "");
        String billingAddressId = mHelper.setProfile(profile);
        PersonalDataManager.CreditCard creditCard =
                new PersonalDataManager.CreditCard("", "https://example.com", true, true, "Jon Doe",
                        "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                        CardType.UNKNOWN, billingAddressId, "" /* serverId */);
        mHelper.setCreditCard(creditCard);

        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator =
                TestThreadUtils.runOnUiThreadBlocking(() -> createPaymentRequestCoordinator(model));
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        /* Request all PR sections. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            model.set(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
        });

        /* Non-empty sections should not display the 'add' button in their title. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        /* Non-empty sections should not be 'fixed', i.e., they can be expanded. */
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(isDisplayed()));

        /* All section dividers are visible. */
        for (View divider : viewHolder.mDividers) {
            onView(is(divider)).check(matches(isDisplayed()));
        }

        /* Check contents of sections. */
        AssistantChoiceList contactsList = TestThreadUtils.runOnUiThreadBlocking(
                () -> viewHolder.mContactSection.findViewById(R.id.section_choice_list));
        AssistantChoiceList paymentsList = TestThreadUtils.runOnUiThreadBlocking(
                () -> viewHolder.mPaymentSection.findViewById(R.id.section_choice_list));
        AssistantChoiceList shippingList = TestThreadUtils.runOnUiThreadBlocking(
                () -> viewHolder.mShippingSection.findViewById(R.id.section_choice_list));

        assertThat(contactsList.getItemCount(), is(1));
        assertThat(paymentsList.getItemCount(), is(1));
        assertThat(shippingList.getItemCount(), is(1));

        testContact("maggie@simpson.com", "Maggie Simpson\nmaggie@simpson.com",
                viewHolder.mContactSection.getCollapsedView(), contactsList.getItem(0));
        testPaymentMethod("1111", "Jon Doe", "12/2050",
                viewHolder.mPaymentSection.getCollapsedView(), paymentsList.getItem(0));
        testShippingAddress("Maggie Simpson", "Acme Inc., 123 Main, 90210 Los Angeles, California",
                "Acme Inc., 123 Main, 90210 Los Angeles, California, Uzbekistan",
                viewHolder.mShippingSection.getCollapsedView(), shippingList.getItem(0));
    }

    private void testContact(String expectedContactSummary, String expectedContactFullDescription,
            View summaryView, View fullView) {
        onView(allOf(withId(R.id.contact_summary), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedContactSummary)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(summaryView))))
                .check(matches(not(isDisplayed())));

        onView(allOf(withId(R.id.contact_full), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedContactFullDescription)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(fullView))))
                .check(matches(not(isDisplayed())));
    }

    private void testPaymentMethod(String expectedObfuscatedCardNumber, String expectedCardName,
            String expectedCardExpiration, View summaryView, View fullView) {
        onView(allOf(withId(R.id.credit_card_number), isDescendantOfA(is(summaryView))))
                .check(matches(withText(containsString(expectedObfuscatedCardNumber))));
        onView(allOf(withId(R.id.credit_card_expiration), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedCardExpiration)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(summaryView))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.credit_card_name), isDescendantOfA(is(summaryView))))
                .check(doesNotExist());

        onView(allOf(withId(R.id.credit_card_number), isDescendantOfA(is(fullView))))
                .check(matches(withText(containsString(expectedObfuscatedCardNumber))));
        onView(allOf(withId(R.id.credit_card_expiration), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedCardExpiration)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(fullView))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.credit_card_name), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedCardName)));
    }

    private void testShippingAddress(String expectedFullName, String expectedShortAddress,
            String expectedFullAddress, View summaryView, View fullView) {
        onView(allOf(withId(R.id.full_name), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedFullName)));
        onView(allOf(withId(R.id.short_address), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedShortAddress)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(summaryView))))
                .check(matches(not(isDisplayed())));

        onView(allOf(withId(R.id.full_name), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedFullName)));
        onView(allOf(withId(R.id.full_address), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedFullAddress)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(fullView))))
                .check(matches(not(isDisplayed())));
    }
}
