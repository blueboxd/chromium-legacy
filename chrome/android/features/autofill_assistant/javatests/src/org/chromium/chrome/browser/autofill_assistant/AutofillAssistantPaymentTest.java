// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.scrollTo;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withTagValue;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto.TermsAndConditionsState;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FocusElementProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto.InputType;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseCreditCardProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UserFormSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.VisibilityRequirement;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant payment.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPaymentTest {
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
     * Fill a form with a saved credit card's details and type the CVC when prompted.
     */
    @Test
    @MediumTest
    public void testEnterPayment() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setRequestPaymentMethod(true)
                                                    .addSupportedBasicCardNetworks("visa")
                                                    .setPrivacyNoticeText("3rd party privacy text")
                                                    .setShowTermsAsCheckbox(true)
                                                    .setRequestTermsAndConditions(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setUseCard(UseCreditCardProto.newBuilder().setFormFieldElement(
                                 ElementReferenceProto.newBuilder().addSelectors("#card_number")))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(
                             is(AssistantTagsForTesting
                                             .COLLECT_USER_DATA_CHECKBOX_TERMS_SECTION_TAG))),
                       withId(R.id.collect_data_privacy_notice)))
                .check(matches(isDisplayed()));

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.card_unmask_input), isCompletelyDisplayed());
        onView(withId(R.id.card_unmask_input)).perform(typeText("123"));
        onView(withId(R.id.positive_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue("name", getWebContents()), is("John Doe"));
        assertThat(getElementValue("card_number", getWebContents()), is("4111111111111111"));
        assertThat(getElementValue("cv2_number", getWebContents()), is("123"));
        assertThat(getElementValue("exp_month", getWebContents()), is("12"));
        assertThat(getElementValue("exp_year", getWebContents()), is("2050"));
    }

    /**
     * Fill a form with a password generated by Chrome.
     */
    @Test
    @MediumTest
    public void testEnterGeneratedPassword() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setSetFormValue(
                                SetFormFieldValueProto.newBuilder()
                                        .addValue(SetFormFieldValueProto.KeyPress.newBuilder()
                                                          .setGeneratePassword(true))
                                        .setElement(ElementReferenceProto.newBuilder().addSelectors(
                                                "#password")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Password generation")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue("password", getWebContents()).length(), greaterThan(0));
    }

    /**
     * Showcasts an element of the webpage and checks that it can be interacted with.
     */
    @Test
    @MediumTest
    public void testTermsAndConditionsWithFocusElement() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setFocusElement(
                                FocusElementProto.newBuilder()
                                        .setElement(ElementReferenceProto.newBuilder().addSelectors(
                                                "div.terms"))
                                        .setTouchableElementArea(
                                                ElementAreaProto.newBuilder().addTouchable(
                                                        Rectangle.newBuilder().addElements(
                                                                ElementReferenceProto.newBuilder()
                                                                        .addSelectors(
                                                                                "div.terms")))))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setRequestPaymentMethod(true)
                                                    .addSupportedBasicCardNetworks("visa")
                                                    .setPrivacyNoticeText("3rd party privacy text")
                                                    .setShowTermsAsCheckbox(true)
                                                    .setRequestTermsAndConditions(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());
        Choice toggle_chip = Choice.newBuilder()
                                     .setChip(ChipProto.newBuilder().setText("Toggle"))
                                     .addShowOnlyIfElementExists(
                                             ElementReferenceProto.newBuilder()
                                                     .addSelectors("div#toggle_on")
                                                     .setVisibilityRequirement(
                                                             VisibilityRequirement.MUST_BE_VISIBLE))
                                     .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Finish")
                                            .addChoices(Choice.newBuilder())
                                            .addChoices(toggle_chip))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Continue")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isDisplayed());
        tapElement("button", mTestRule);
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Toggle"), isDisplayed());

        // Verify that in the next step the touchable window is not present anymore.
        tapElement("button", mTestRule);
        onView(withText("Toggle")).check(matches(isDisplayed()));
    }

    /**
     * Check that sending an empty privacy notice text removes the privacy notice section.
     */
    @Test
    @MediumTest
    public void testRemovePrivacyNotice() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setPrivacyNoticeText("")
                                                    .setRequestTermsAndConditions(true)
                                                    .setShowTermsAsCheckbox(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Continue")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        onView(allOf(isDescendantOfA(withTagValue(
                             is(AssistantTagsForTesting
                                             .COLLECT_USER_DATA_CHECKBOX_TERMS_SECTION_TAG))),
                       withId(R.id.collect_data_privacy_notice)))
                .check(matches(not(isDisplayed())));
    }

    /**
     * If the user taps away from a text field, the keyboard should become hidden
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1041870")
    public void testKeyboardIsHiddenOnLostFocus() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        UserFormSectionProto userFormSectionProto =
                UserFormSectionProto.newBuilder()
                        .setTitle("User form")
                        .setTextInputSection(
                                TextInputSectionProto.newBuilder()
                                        .addInputFields(TextInputProto.newBuilder()
                                                                .setHint("Field 1")
                                                                .setInputType(InputType.INPUT_TEXT)
                                                                .setClientMemoryKey("field_1"))
                                        .addInputFields(TextInputProto.newBuilder()
                                                                .setHint("Field 2")
                                                                .setInputType(InputType.INPUT_TEXT)
                                                                .setClientMemoryKey("field_2")))
                        .build();

        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setPrivacyNoticeText("3rd party privacy text")
                                         .setShowTermsAsCheckbox(true)
                                         .setRequestTermsAndConditions(true)
                                         .setAcceptTermsAndConditionsText("accept terms")
                                         .setTermsAndConditionsState(
                                                 TermsAndConditionsState.ACCEPTED)
                                         .addAdditionalPrependedSections(userFormSectionProto))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("User form"), isDisplayed());
        onView(withText("User form")).perform(click());
        waitUntilViewMatchesCondition(withText("Field 1"), isDisplayed());
        onView(withText("Field 1")).perform(click());
        waitUntilKeyboardMatchesCondition(mTestRule, true);
        onView(withText("Field 2")).perform(scrollTo(), click());
        waitUntilKeyboardMatchesCondition(mTestRule, true);
        onView(withText("User form")).perform(scrollTo(), click());
        waitUntilKeyboardMatchesCondition(mTestRule, false);
    }

    @Test
    @MediumTest
    public void testIncompleteAddressOnCompleteCard() throws Exception {
        PersonalDataManager.AutofillProfile mockProfile = new PersonalDataManager.AutofillProfile(
                /* guid= */ "",
                /* origin= */ "https://www.example.com", "John Doe", /* companyName= */ "",
                "Somestreet",
                /* region= */ "", "Switzerland", "", /* postalCode= */ "", /* sortingCode= */ "",
                "CH", "+41 79 123 45 67", "johndoe@google.com", /* languageCode= */ "");
        String profileId = mHelper.setProfile(mockProfile);
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setRequestPaymentMethod(true)
                                                    .addSupportedBasicCardNetworks("visa")
                                                    .setShowTermsAsCheckbox(true)
                                                    .setRequestTermsAndConditions(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Check our UI.
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withContentDescription("Continue")).check(matches(not(isEnabled())));
        onView(allOf(withId(R.id.incomplete_error),
                       withParent(withId(R.id.payment_method_summary))))
                .check(matches(allOf(isDisplayed(), withText("Information missing"))));
        onView(withId(R.id.payment_method_summary)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.payment_method_full), isDisplayed());

        // Check CardEditor UI.
        onView(withContentDescription("Edit card")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Card number*"), allOf(isDisplayed(), isEnabled()));
        onView(withText(containsString("John Doe, Somestreet")))
                .check(matches(withText(containsString("Enter a valid address"))));
    }
}
