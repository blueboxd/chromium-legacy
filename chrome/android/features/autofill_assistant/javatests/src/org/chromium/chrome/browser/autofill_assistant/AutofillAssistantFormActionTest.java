// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.hasDescendant;
import static android.support.test.espresso.matcher.ViewMatchers.hasSibling;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.CounterInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FormInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FormProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectionInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowFormProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant bottomsheet.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantFormActionTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    @Before
    public void setUp() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mTestRule.getActivity().getScrim().disableAnimationForTesting(true);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testFormAction() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // FromProto.Builder, extracted to avoid excessive line widths.
        FormProto.Builder formProto =
                FormProto.newBuilder()
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder()
                                        .addCounters(CounterInputProto.Counter.newBuilder()
                                                             .setMinValue(1)
                                                             .setMaxValue(9)
                                                             .setLabel("Counter 1")
                                                             .setDescriptionLine1("$34.00 per item")
                                                             .setDescriptionLine2(
                                                                     "<link1>Details</link1>"))
                                        .addCounters(
                                                CounterInputProto.Counter.newBuilder()
                                                        .setMinValue(1)
                                                        .setMaxValue(9)
                                                        .setLabel("Counter 2")
                                                        .setDescriptionLine1("$387.00 per item"))
                                        .setMinimizedCount(1)
                                        .setMinCountersSum(2)
                                        .setMinimizeText("Minimize")
                                        .setExpandText("Expand")))
                        .addInputs(FormInputProto.newBuilder().setSelection(
                                SelectionInputProto.newBuilder()
                                        .addChoices(
                                                SelectionInputProto.Choice.newBuilder()
                                                        .setLabel("Choice 1")
                                                        .setDescriptionLine1("$10.00 per choice")
                                                        .setDescriptionLine2(
                                                                "<link1>Details</link1>"))
                                        .setAllowMultiple(true)))
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder().addCounters(
                                        CounterInputProto.Counter.newBuilder()
                                                .setMinValue(1)
                                                .setMaxValue(9)
                                                .setLabel("Counter 3")
                                                .setDescriptionLine1("$20.00 per item")
                                                .setDescriptionLine2("<link1>Details</link1>"))));
        // FormAction.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Continue"))
                                              .setForm(formProto))
                         .build());

        // Prompt.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Finished")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("End"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        // TODO(b/144690738) Remove the isDisplayed() condition.
        onView(allOf(isDisplayed(), withId(R.id.increase_button),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .perform(click());

        onView(allOf(isDisplayed(), withId(R.id.expand_label))).perform(click());
        onView(allOf(isDisplayed(), withId(R.id.increase_button),
                       hasSibling(hasDescendant(withText("Counter 3")))))
                .perform(click());

        onView(allOf(isDisplayed(), withId(R.id.checkbox),
                       hasSibling(hasDescendant(withText("Choice 1")))))
                .perform(click());
        onView(allOf(isDisplayed(), withId(R.id.increase_button),
                       hasSibling(hasDescendant(withText("Counter 2")))))
                .perform(click());

        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("End"), isCompletelyDisplayed());
        // TODO(b/144978160): check that the values were correctly written to the action response.
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testFormActionClickLink() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // FromProto.Builder, extracted to avoid excessive line widths.
        FormProto.Builder formProto =
                FormProto.newBuilder().addInputs(FormInputProto.newBuilder().setCounter(
                        CounterInputProto.newBuilder()
                                .addCounters(CounterInputProto.Counter.newBuilder()
                                                     .setMinValue(1)
                                                     .setMaxValue(9)
                                                     .setLabel("Counter 1")
                                                     .setDescriptionLine1("$34.00 per item")
                                                     .setDescriptionLine2("<link4>Details</link4>"))
                                .addCounters(CounterInputProto.Counter.newBuilder()
                                                     .setMinValue(1)
                                                     .setMaxValue(9)
                                                     .setLabel("Counter 2")
                                                     .setDescriptionLine1("$387.00 per item"))
                                .setMinimizedCount(1)
                                .setMinCountersSum(2)
                                .setMinimizeText("Minimize")
                                .setExpandText("Expand")));
        // FormAction.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Continue"))
                                              .setForm(formProto))
                         .build());

        // Prompt.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Finished")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("End"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        // TODO(b/144690738) Remove the isDisplayed() condition.

        onView(allOf(isDisplayed(), withText("Details"))).perform(click());
        // TODO(b/144978160) Check that the correct link number was written to the action response.

        waitUntilViewMatchesCondition(withText("End"), isCompletelyDisplayed());
    }
}
