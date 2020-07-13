// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;

import android.support.test.InstrumentationRegistry;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.FocusElementProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests autofill assistant in a normal Chrome tab.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantChromeTabIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();

    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE_A = "autofill_assistant_target_website.html";
    private static final String TEST_PAGE_B = "form_target_website.html";

    private EmbeddedTestServer mTestServer;

    private String getURL(String page) {
        return mTestServer.getURL(HTML_DIRECTORY + page);
    }

    private void setupScripts(AutofillAssistantTestScript... scripts) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Arrays.asList(scripts));
        testService.scheduleForInjection();
    }

    private void startAutofillAssistantOnTab(String pageToLoad) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantFacade.start(mTestRule.getActivity(),
                                /* bundleExtras= */ null, getURL(pageToLoad)));
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestRule.startMainActivityWithURL(getURL(TEST_PAGE_A));
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    // Restricted to phones due to https://crbug.com/429671
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void newTabButtonHidesAndRecoversAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Prompt")
                                            .setDisableForceExpandSheet(true)
                                            .addChoices(PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);

        setupScripts(script);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        onView(withClassName(is(ScrimView.class.getName())))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));

        onView(withId(org.chromium.chrome.R.id.tab_switcher_button)).perform(click());
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        onView(withClassName(is(ScrimView.class.getName()))).check(doesNotExist());

        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        onView(withClassName(is(ScrimView.class.getName())))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
    }

    @Test
    @MediumTest
    public void switchingTabHidesAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);

        int initialTabId =
                TabModelUtils.getCurrentTabId(mTestRule.getActivity().getCurrentTabModel());

        setupScripts(script);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        ChromeTabUtils.switchTabInCurrentTabModel(mTestRule.getActivity(),
                TabModelUtils.getTabIndexById(
                        mTestRule.getActivity().getCurrentTabModel(), initialTabId));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void closingTabResurfacesAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupScripts(script);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mTestRule.getActivity());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void startingNewAutofillAssistantChangeTabResumeRunOnPreviousTab() {
        ArrayList<ActionProto> listA = new ArrayList<>();
        listA.add((ActionProto) ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt A")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptA = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                listA);

        ArrayList<ActionProto> listB = new ArrayList<>();
        listB.add((ActionProto) ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt B")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptB = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_B)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                listB);

        int initialTabId =
                TabModelUtils.getCurrentTabId(mTestRule.getActivity().getCurrentTabModel());

        setupScripts(scriptA, scriptB);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt A"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        startAutofillAssistantOnTab(TEST_PAGE_B);
        waitUntilViewMatchesCondition(withText("Prompt B"), isCompletelyDisplayed());

        ChromeTabUtils.switchTabInCurrentTabModel(mTestRule.getActivity(),
                TabModelUtils.getTabIndexById(
                        mTestRule.getActivity().getCurrentTabModel(), initialTabId));
        waitUntilViewAssertionTrue(withText("Prompt B"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void startingNewAutofillAssistantCloseTabResumesRunOnPreviousTab() {
        ArrayList<ActionProto> listA = new ArrayList<>();
        listA.add((ActionProto) ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt A")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptA = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                listA);

        ArrayList<ActionProto> listB = new ArrayList<>();
        listB.add((ActionProto) ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt B")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptB = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_B)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                listB);
        setupScripts(scriptA, scriptB);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt A"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        startAutofillAssistantOnTab(TEST_PAGE_B);
        waitUntilViewMatchesCondition(withText("Prompt B"), isCompletelyDisplayed());

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mTestRule.getActivity());
        waitUntilViewAssertionTrue(withText("Prompt B"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void backButtonDestroysAutofillAssistantUi() throws Exception {
        ChromeTabUtils.loadUrlOnUiThread(
                mTestRule.getActivity().getActivityTab(), getURL(TEST_PAGE_B));

        SelectorProto element =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(
                                SelectorProto.Filter.newBuilder().setCssSelector("#profile_name"))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setFocusElement(FocusElementProto.newBuilder()
                                                 .setElement(element)
                                                 .setTouchableElementArea(
                                                         ElementAreaProto.newBuilder().addTouchable(
                                                                 Rectangle.newBuilder().addElements(
                                                                         element))))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupScripts(script);
        startAutofillAssistantOnTab(TEST_PAGE_B);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Force the keyboard to open.
        tapElement(mTestRule, "profile_name");
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);

        // First press on back button closes the keyboard.
        Espresso.pressBack();
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ false);
        onView(withText("Prompt")).check(matches(isCompletelyDisplayed()));

        // Second press on back button destroys Autofill Assistant UI.
        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText(R.string.undo), isCompletelyDisplayed());
        onView(withId(R.id.autofill_assistant)).check(doesNotExist());
        assertThat(mTestRule.getActivity().getActivityTab().getUrl().getSpec(),
                is(getURL(TEST_PAGE_B)));

        // Third press on back button navigates back.
        Espresso.pressBack();
        waitUntil(()
                          -> mTestRule.getActivity().getActivityTab().getUrl().getSpec().equals(
                                  getURL(TEST_PAGE_A)));
    }

    @Test
    @MediumTest
    public void backButtonInStoppedAutofillAssistantState() {
        ChromeTabUtils.loadUrlOnUiThread(
                mTestRule.getActivity().getActivityTab(), getURL(TEST_PAGE_B));

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder().setMessage("Shutdown"))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder().setStop(StopProto.newBuilder()).build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupScripts(script);
        startAutofillAssistantOnTab(TEST_PAGE_B);

        waitUntilViewMatchesCondition(withText("Shutdown"), isCompletelyDisplayed());

        // First press on back button fully destroys Autofill Assistant UI, without Undo.
        Espresso.pressBack();
        waitUntilViewAssertionTrue(
                withId(R.id.autofill_assistant), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        onView(withText("Shutdown")).check(doesNotExist());
        onView(withText(R.string.undo)).check(doesNotExist());
        assertThat(mTestRule.getActivity().getActivityTab().getUrl().getSpec(),
                is(getURL(TEST_PAGE_B)));

        // Second press on back button navigates back.
        Espresso.pressBack();
        waitUntil(()
                          -> mTestRule.getActivity().getActivityTab().getUrl().getSpec().equals(
                                  getURL(TEST_PAGE_A)));
    }

    @Test
    @MediumTest
    public void backButtonIsIgnoredInBrowseMode() {
        // Same domain, different page, such that navigating back is allowed in the BROWSE state.
        ChromeTabUtils.loadUrlOnUiThread(
                mTestRule.getActivity().getActivityTab(), getURL(TEST_PAGE_B));

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Prompt")
                                            .setBrowseMode(true)
                                            .addChoices(PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupScripts(script);
        startAutofillAssistantOnTab(TEST_PAGE_B);

        // BROWSE state must not automatically collapse the UI.
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // First press on back button collapses Autofill Assistant UI.
        Espresso.pressBack();
        waitUntilViewMatchesCondition(
                withId(R.id.status_message), allOf(withText("Prompt"), not(isDisplayed())));
        onView(withId(R.id.autofill_assistant)).check(matches(isDisplayed()));

        // Second press on back button navigates back, without removing the Autofill Assistannt UI.
        Espresso.pressBack();
        waitUntil(()
                          -> mTestRule.getActivity().getActivityTab().getUrl().getSpec().equals(
                                  getURL(TEST_PAGE_A)));
        onView(withId(R.id.autofill_assistant)).check(matches(isDisplayed()));
        onView(withId(R.id.status_message)).check(matches(withText("Prompt")));
    }

    @Test
    @MediumTest
    public void interactingWithLocationBarHidesAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupScripts(script);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Clicking location bar hides UI and shows the keyboard.
        onView(withId(org.chromium.chrome.R.id.url_bar)).perform(click());
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        ChromeTabbedActivity activity = mTestRule.getActivity();
        assertThat(activity.getWindowAndroid().getKeyboardDelegate().isKeyboardShowing(
                           activity, activity.getCompositorViewHolder()),
                is(true));

        // Closing keyboard brings it back.
        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Committing URL shows error.
        onView(withId(org.chromium.chrome.R.id.url_bar))
                .perform(click(), typeText(getURL(TEST_PAGE_B)), pressImeActionButton());
        waitUntilViewMatchesCondition(withText(containsString("Sorry")), isCompletelyDisplayed());
        waitUntil(()
                          -> mTestRule.getActivity().getActivityTab().getUrl().getSpec().equals(
                                  getURL(TEST_PAGE_B)));
    }
}
