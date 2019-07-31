// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
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

import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.PAYMENT_REQUEST_TERMS_REQUIRE_REVIEW;
import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.VERTICAL_EXPANDER_CHEVRON;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPaymentRequestTestHelper.ViewHolder;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantChoiceList;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestModel;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantTermsAndConditionsState;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the Autofill Assistant payment request UI.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPaymentRequestUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private AutofillAssistantPaymentRequestTestHelper mHelper;

    @Before
    public void setUp() throws Exception {
        AutofillAssistantUiTestUtil.startOnBlankPage(mTestRule);
        mHelper = new AutofillAssistantPaymentRequestTestHelper();
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantPaymentRequestCoordinator createPaymentRequestCoordinator(
            AssistantPaymentRequestModel model) throws Exception {
        AssistantPaymentRequestCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AssistantPaymentRequestCoordinator(mTestRule.getActivity(), model));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantUiTestUtil.attachToCoordinator(
                                mTestRule.getActivity(), coordinator.getView()));
        return coordinator;
    }

    /**
     * Test assumptions about the initial state of the payment request.
     */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);

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
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);
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
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);
        AutofillAssistantPaymentRequestTestHelper.MockDelegate delegate =
                new AutofillAssistantPaymentRequestTestHelper.MockDelegate();
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
            model.set(AssistantPaymentRequestModel.DELEGATE, delegate);
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

        /* Test delegate status. */
        assertThat(delegate.mPaymentMethod, nullValue());
        assertThat(delegate.mContact, nullValue());
        assertThat(delegate.mAddress, nullValue());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));
    }

    /**
     * Shows a payment request, then adds a new contact to the personal data manager.
     * Tests whether the new contact is added to the payment request.
     */
    @Test
    @MediumTest
    public void testContactDetailsLiveUpdate() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // WEB_CONTENTS are necessary for the creation of the editors.
            model.set(AssistantPaymentRequestModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
        });

        /* Contact details section should be empty. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        AssistantChoiceList contactsList = TestThreadUtils.runOnUiThreadBlocking(
                () -> viewHolder.mContactSection.findViewById(R.id.section_choice_list));
        assertThat(contactsList.getItemCount(), is(0));

        /* Add profile to the personal data manager. */
        String profileId = mHelper.addDummyProfile("John Doe", "john@gmail.com");

        /* Contact details section should now contain and have pre-selected the new contact. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        assertThat(contactsList.getItemCount(), is(1));
        onView(allOf(withId(R.id.contact_summary),
                       isDescendantOfA(is(viewHolder.mContactSection.getCollapsedView()))))
                .check(matches(withText("john@gmail.com")));

        /* Remove profile from personal data manager. Section should be empty again. */
        mHelper.deleteProfile(profileId);
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        assertThat(contactsList.getItemCount(), is(0));

        /* Tap the 'add' button to open the editor, to make sure that it still works. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .perform(click());
        onView(withId(R.id.editor_container)).check(matches(isDisplayed()));
    }

    /**
     * Shows a payment request, then adds a new payment method to the personal data manager.
     * Tests whether the new payment method is added to the payment request.
     */
    @Test
    @MediumTest
    public void testPaymentMethodsLiveUpdate() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // WEB_CONTENTS are necessary for the creation of the editors.
            model.set(AssistantPaymentRequestModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
        });

        /* Payment method section should be empty and show the 'add' button in the title. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        AssistantChoiceList paymentsList = TestThreadUtils.runOnUiThreadBlocking(
                () -> viewHolder.mPaymentSection.findViewById(R.id.section_choice_list));
        assertThat(paymentsList.getItemCount(), is(0));

        /* Add profile and credit card to the personal data manager. */
        String billingAddressId = mHelper.addDummyProfile("Jill Doe", "jill@gmail.com");
        String creditCardId = mHelper.addDummyCreditCard(billingAddressId);

        /* Payment method section contains the new credit card, which should be pre-selected. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        assertThat(paymentsList.getItemCount(), is(1));
        onView(allOf(withId(R.id.credit_card_name), isDescendantOfA(is(paymentsList.getItem(0)))))
                .check(matches(withText("Jill Doe")));

        /* Remove credit card from personal data manager. Section should be empty again. */
        mHelper.deleteCreditCard(creditCardId);
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        assertThat(paymentsList.getItemCount(), is(0));

        /* Tap the 'add' button to open the editor, to make sure that it still works. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .perform(click());
        onView(withId(R.id.editor_container)).check(matches(isDisplayed()));
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
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);
        AutofillAssistantPaymentRequestTestHelper.MockDelegate delegate =
                new AutofillAssistantPaymentRequestTestHelper.MockDelegate();
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
            model.set(AssistantPaymentRequestModel.DELEGATE, delegate);
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

        /* Check delegate status. */
        assertThat(delegate.mPaymentMethod.getCard().getNumber(), is("4111111111111111"));
        assertThat(delegate.mPaymentMethod.getCard().getName(), is("Jon Doe"));
        assertThat(delegate.mPaymentMethod.getCard().getBasicCardIssuerNetwork(), is("visa"));
        assertThat(delegate.mPaymentMethod.getCard().getBillingAddressId(), is(billingAddressId));
        assertThat(delegate.mPaymentMethod.getCard().getMonth(), is("12"));
        assertThat(delegate.mPaymentMethod.getCard().getYear(), is("2050"));
        assertThat(delegate.mContact.getPayerName(), is("Maggie Simpson"));
        assertThat(delegate.mContact.getPayerEmail(), is("maggie@simpson.com"));
        assertThat(delegate.mAddress.getProfile().getFullName(), is("Maggie Simpson"));
        assertThat(delegate.mAddress.getProfile().getStreetAddress(), containsString("123 Main"));
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));
    }

    /**
     * When the last contact info, payment method or shipping address is removed from the personal
     * data manager, the user's selection has implicitly changed (from whatever it was before to
     * null).
     */
    @Test
    @MediumTest
    public void testRemoveLastItemImplicitSelection() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);
        AutofillAssistantPaymentRequestTestHelper.MockDelegate delegate =
                new AutofillAssistantPaymentRequestTestHelper.MockDelegate();
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        /* Add complete profile and credit card to the personal data manager. */
        String profileId = mHelper.addDummyProfile("John Doe", "john@gmail.com");
        String creditCardId = mHelper.addDummyCreditCard(profileId);

        /* Request all PR sections. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantPaymentRequestModel.REQUEST_NAME, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PHONE, true);
            model.set(AssistantPaymentRequestModel.REQUEST_EMAIL, true);
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            model.set(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantPaymentRequestModel.DELEGATE, delegate);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
        });

        /* Profile and payment method should be automatically selected. */
        assertThat(delegate.mContact, not(nullValue()));
        assertThat(delegate.mAddress, not(nullValue()));
        assertThat(delegate.mPaymentMethod, not(nullValue()));

        // Remove payment method and profile
        mHelper.deleteCreditCard(creditCardId);
        mHelper.deleteProfile(profileId);

        // Note: before asserting that the delegate was updated, we need to ensure that the
        // UI thread has processed all events.
        onView(is(coordinator.getView())).check(matches(isDisplayed()));

        assertThat(delegate.mContact, nullValue());
        assertThat(delegate.mAddress, nullValue());
        assertThat(delegate.mPaymentMethod, nullValue());
    }

    @Test
    @MediumTest
    public void testTermsAndConditions() throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        createPaymentRequestCoordinator(model);
        AutofillAssistantPaymentRequestTestHelper.MockDelegate delegate =
                new AutofillAssistantPaymentRequestTestHelper.MockDelegate();

        String acceptTermsText = "I accept";

        // Display terms as 2 radio buttons "I accept" vs "I don't".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(
                    AssistantPaymentRequestModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT, acceptTermsText);
            model.set(AssistantPaymentRequestModel.SHOW_TERMS_AS_CHECKBOX, false);
            model.set(AssistantPaymentRequestModel.DELEGATE, delegate);

            // Setting web contents will set the origin and the decline terms text.
            model.set(AssistantPaymentRequestModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
        });

        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));

        // Adding #isDisplayed as a requirement makes sure only one of the accept terms text is
        // shown (plus #onView requires the matcher to match exactly one view).
        Matcher<View> acceptMatcher = allOf(withText(acceptTermsText), isDisplayed());
        Matcher<View> declineMatcher = withTagValue(is(PAYMENT_REQUEST_TERMS_REQUIRE_REVIEW));

        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.ACCEPTED));

        // Second click on accept doesn't change the state.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.ACCEPTED));

        onView(declineMatcher).check(matches(isDisplayed())).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.REQUIRES_REVIEW));

        // Display the terms as a single checbox.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantPaymentRequestModel.SHOW_TERMS_AS_CHECKBOX, true));

        // The decline choice is not shown.
        onView(declineMatcher).check(matches(not(isDisplayed())));

        // First click marks the terms as accepted.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.ACCEPTED));

        // Second click marks the terms as not selected.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));

        // Change the "I accept" text to be a clickable link.
        String acceptTermsText2 =
                "<link42>I accept</link42>"; // second variable is necessary because used in lambda
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantPaymentRequestModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT,
                                acceptTermsText2));
        acceptMatcher = allOf(withText(acceptTermsText), isDisplayed());

        // Clicking the text will trigger the link.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mLastLinkClicked, is(42));
    }

    /**
     * Test that if the billing address does not have a postal code and the postal code is required,
     * an error message is displayed.
     */
    @Test
    @MediumTest
    public void testCreditCardWithoutPostcode() throws Exception {
        // add credit card without postcode.
        String profileId = mHelper.addDummyProfile("John Doe", "john@gmail.com", "");
        mHelper.addDummyCreditCard(profileId);

        // setup the view to require a billing postcode.
        AutofillAssistantPaymentRequestTestHelper.ViewHolder viewHolder =
                setupCreditCardPostalCodeTest(/* requireBillingPostalCode: */ true);

        // check that the card is not accepted (i.e. an error message is shown).
        onView(is(getPaymentSummaryErrorView(viewHolder))).check(matches(isDisplayed()));
        onView(is(getPaymentSummaryErrorView(viewHolder)))
                .check(matches(withText("Billing postcode missing")));

        // setup the view to not require a billing postcode.
        // TODO: clean previous view.
        viewHolder = setupCreditCardPostalCodeTest(/* requireBillingPostalCode: */ false);

        // check that the card is now accepted.
        onView(is(getPaymentSummaryErrorView(viewHolder))).check(matches(not(isDisplayed())));
    }

    /**
     * Test that requiring a billing postal code for a billing address that has it does not display
     * an error message.
     */
    @Test
    @MediumTest
    public void testCreditCardWithPostcode() throws Exception {
        // setup a card with a postcode.
        String profileId = mHelper.addDummyProfile("Jane Doe", "jane@gmail.com", "98004");
        mHelper.addDummyCreditCard(profileId);

        // setup the view to require a billing postcode.
        AutofillAssistantPaymentRequestTestHelper.ViewHolder viewHolder =
                setupCreditCardPostalCodeTest(/* requireBillingPostalCode: */ true);

        // check that the card is accepted.
        onView(is(getPaymentSummaryErrorView(viewHolder))).check(matches(not(isDisplayed())));
    }

    private AutofillAssistantPaymentRequestTestHelper.ViewHolder setupCreditCardPostalCodeTest(
            boolean requireBillingPostalCode) throws Exception {
        AssistantPaymentRequestModel model = new AssistantPaymentRequestModel();
        AssistantPaymentRequestCoordinator coordinator = createPaymentRequestCoordinator(model);
        AutofillAssistantPaymentRequestTestHelper.MockDelegate delegate =
                new AutofillAssistantPaymentRequestTestHelper.MockDelegate();
        AutofillAssistantPaymentRequestTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantPaymentRequestTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantPaymentRequestModel.REQUIRE_BILLING_POSTAL_CODE,
                    requireBillingPostalCode);
            model.set(AssistantPaymentRequestModel.BILLING_POSTAL_CODE_MISSING_TEXT,
                    "Billing postcode missing");
            model.set(AssistantPaymentRequestModel.REQUEST_PAYMENT, true);
            model.set(AssistantPaymentRequestModel.DELEGATE, delegate);
            model.set(AssistantPaymentRequestModel.VISIBLE, true);
        });

        return viewHolder;
    }

    private View getPaymentSummaryErrorView(ViewHolder viewHolder) {
        return viewHolder.mPaymentSection.findViewById(R.id.payment_method_summary)
                .findViewById(R.id.incomplete_error);
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
