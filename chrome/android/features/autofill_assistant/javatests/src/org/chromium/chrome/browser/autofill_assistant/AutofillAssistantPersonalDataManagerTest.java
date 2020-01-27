// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.clearText;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasDescendant;
import static android.support.test.espresso.matcher.ViewMatchers.isChecked;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isNextAfterSibling;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.widget.RadioButton;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ContactDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseAddressProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseAddressProto.RequiredField;
import org.chromium.chrome.browser.autofill_assistant.proto.UseAddressProto.RequiredField.AddressField;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant's interaction with the PersonalDataManager.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPersonalDataManagerTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "form_target_website.html";

    private AutofillAssistantCollectUserDataTestHelper mHelper;

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mHelper = new AutofillAssistantCollectUserDataTestHelper();
    }

    /**
     * Add a contact with Autofill Assistant UI and fill it into the form.
     */
    @Test
    @MediumTest
    public void testCreateAndEnterProfile() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setUseAddress(
                                 UseAddressProto.newBuilder()
                                         .setName("contact")
                                         .setFormFieldElement(
                                                 ElementReferenceProto.newBuilder().addSelectors(
                                                         "#profile_name"))
                                         .addRequiredFields(
                                                 RequiredField.newBuilder()
                                                         .setAddressField(AddressField.FULL_NAME)
                                                         .setElement(
                                                                 ElementReferenceProto.newBuilder()
                                                                         .addSelectors(
                                                                                 "#profile_name")))
                                         .addRequiredFields(
                                                 RequiredField.newBuilder()
                                                         .setAddressField(AddressField.EMAIL)
                                                         .setElement(
                                                                 ElementReferenceProto.newBuilder()
                                                                         .addSelectors("#email"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(typeText("John Doe"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*")).perform(typeText("johndoe@google.com"));
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withId(R.id.contact_summary))
                .check(matches(allOf(withText("johndoe@google.com"), isDisplayed())));
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue("profile_name", getWebContents()), is("John Doe"));
        assertThat(getElementValue("email", getWebContents()), is("johndoe@google.com"));
    }

    /**
     * Catch the insert of a profile added outside of the Autofill Assistant, e.g. with the Chrome
     * settings UI, and fill it into the form.
     */
    @Test
    @MediumTest
    public void testExternalAddAndEnterProfile() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        ElementReferenceProto.newBuilder().addSelectors(
                                                "#profile_name")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");
        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue("profile_name", getWebContents()), is("John Doe"));
        assertThat(getElementValue("email", getWebContents()), is("johndoe@google.com"));
    }

    /**
     * A new profile added outside of the Autofill Assistant, e.g. with the Chrome settings UI,
     * should not overwrite the current selection.
     */
    @Test
    @MediumTest
    public void testExternalAddNewProfile() throws Exception {
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        ElementReferenceProto.newBuilder().addSelectors(
                                                "#profile_name")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        onView(withText("Continue")).check(matches(isEnabled()));
        // Add new entry that is not supposed to be selected.
        mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(withText(containsString("Adam West")), isDisplayed());
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        // Make sure it's not Adam West that was selected.
        assertThat(getElementValue("profile_name", getWebContents()), is("John Doe"));
        assertThat(getElementValue("email", getWebContents()), is("johndoe@google.com"));
    }

    /**
     * An existing profile deleted outside of the Autofill Assistant, e.g. with the Chrome settings
     * UI, should be removed from the current list.
     */
    @Test
    @MediumTest
    public void testExternalDeleteProfile() throws Exception {
        String profileIdA = mHelper.addDummyProfile("Adam Doe", "adamdoe@google.com");
        String profileIdB = mHelper.addDummyProfile("Berta Doe", "bertadoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("adamdoe@google.com"), isDisplayed()));
        // Delete first profile, expect second to be selected.
        mHelper.deleteProfile(profileIdA);
        waitUntilViewMatchesCondition(withId(R.id.contact_summary),
                allOf(withText("bertadoe@google.com"), isDisplayed()));
        // Delete second profile, expect nothing to be selected.
        mHelper.deleteProfile(profileIdB);
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
    }

    /**
     * Editing the currently selected contact in the Assistant Autofill UI should keep selection.
     */
    @Test
    @MediumTest
    public void testEditOfSelectedProfile() throws Exception {
        mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        ElementReferenceProto.newBuilder().addSelectors(
                                                "#profile_name")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withId(R.id.contact_summary),
                allOf(withText("adamwest@google.com"), isDisplayed()));
        onView(withText("Continue")).check(matches(isEnabled()));
        // Select John Doe.
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(withText(containsString("John Doe")), isDisplayed());
        onView(withText(containsString("John Doe"))).perform(click());
        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        // Edit John Doe to Jane Doe (does not collapse the list after editing).
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(withText(containsString("John Doe")), isDisplayed());
        onView(allOf(withContentDescription("Edit contact info"),
                       isNextAfterSibling(hasDescendant(withText(containsString("John Doe"))))))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(clearText()).perform(typeText("Jane Doe"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*"))
                .perform(clearText())
                .perform(typeText("janedoe@google.com"));
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Contact info"), isDisplayed());
        // Continue.
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        // Make sure it's now Jane Doe.
        assertThat(getElementValue("profile_name", getWebContents()), is("Jane Doe"));
        assertThat(getElementValue("email", getWebContents()), is("janedoe@google.com"));
    }

    // TODO(b/143265578): Add test where credit card is manually entered.

    /**
     * Catch the insert of a credit card added outside of the Autofill Assistant, e.g. with the
     * Chrome settings UI, and fill it into the form.
     */
    @Test
    @MediumTest
    public void testExternalAddCreditCard() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setRequestTermsAndConditions(false))
                         .build());
        // No UseCreditCardAction, that is tested in PaymentTest.
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@google.com");
        mHelper.addDummyCreditCard(profileId);
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
    }

    /**
     * Catch the insert of a credit card with a billing address missing the postal code added
     * outside of the Autofill Assistant, e.g. with the Chrome settings UI, and fill it into the
     * form.
     */
    @Test
    @MediumTest
    public void testExternalAddCreditCardWithoutBillingZip() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setRequestPaymentMethod(true)
                                         .setRequireBillingPostalCode(true)
                                         .setBillingPostalCodeMissingText("Missing Billing Code")
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        String profileId =
                mHelper.addDummyProfile("John Doe", "johndoe@google.com", /* postcode= */ "");
        mHelper.addDummyCreditCard(profileId);
        waitUntilViewMatchesCondition(allOf(withText("Missing Billing Code"),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                isDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
    }

    /**
     * An existing credit card deleted outside of the Autofill Assistant, e.g. with the Chrome
     * settings UI, should be removed from the current list.
     */
    @Test
    @MediumTest
    public void testExternalDeleteCreditCard() throws Exception {
        String profileId;

        profileId = mHelper.addDummyProfile("Adam Doe", "adamdoe@google.com");
        String cardIdA = mHelper.addDummyCreditCard(profileId, "4111111111111111");

        profileId = mHelper.addDummyProfile("Berta Doe", "bertadoe@google.com");
        String cardIdB = mHelper.addDummyCreditCard(profileId, "5555555555554444");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.credit_card_name), withText("Adam Doe")), isDisplayed());
        onView(allOf(withId(R.id.credit_card_name),
                       withParent(allOf(withId(R.id.payment_method_full),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("Adam Doe")));
        onView(withText("Payment method")).perform(click());
        // Delete first card, expect second card to be selected.
        mHelper.deleteCreditCard(cardIdA);
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("4444")), isDisplayed()));
        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.credit_card_name), withText("Berta Doe")), isDisplayed());
        onView(allOf(withId(R.id.credit_card_name),
                       withParent(allOf(withId(R.id.payment_method_full),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("Berta Doe")));
        onView(withText("Payment method")).perform(click());
        // Delete second card, expect nothing to be selected.
        mHelper.deleteCreditCard(cardIdB);
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
    }

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }
}
