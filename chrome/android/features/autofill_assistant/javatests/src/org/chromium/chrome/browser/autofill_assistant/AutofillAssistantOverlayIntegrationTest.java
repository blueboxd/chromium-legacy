// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementOnScreen;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FocusElementProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant's overlay.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantOverlayIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
    }

    /**
     * Tests that clicking on a document element works with a showcast.
     */
    @Test
    @MediumTest
    public void testShowCastOnDocumentElement() throws Exception {
        ElementReferenceProto element = (ElementReferenceProto) ElementReferenceProto.newBuilder()
                                                .addSelectors("#touch_area_one")
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
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_one"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        tapElement(mTestRule, "touch_area_one");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "touch_area_four");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
    }

    /**
     * Tests that clicking on a document element requiring scrolling works with a showcast.
     */
    @Test
    @MediumTest
    public void testShowCastOnDocumentElementInScrolledBrowserWindow() throws Exception {
        ElementReferenceProto element = (ElementReferenceProto) ElementReferenceProto.newBuilder()
                                                .addSelectors("#touch_area_five")
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
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_five"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM. The element should be after a
        // big element forcing the page to scroll.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_five"), is(true));
        tapElement(mTestRule, "touch_area_five");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_five"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "touch_area_six");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_six"), is(true));
    }

    /**
     * Tests that clicking on an iFrame element works with a showcast.
     */
    @Test
    @MediumTest
    public void testShowCastOnIFrameElement() throws Exception {
        ElementReferenceProto element = (ElementReferenceProto) ElementReferenceProto.newBuilder()
                                                .addSelectors("#iframe")
                                                .addSelectors("#touch_area_1")
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
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "iframe", "touch_area_1"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM.
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_1"), is(true));
        tapElement(mTestRule, "iframe", "touch_area_1");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_1"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "iframe", "touch_area_2");
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_2"), is(true));
    }

    /**
     * Tests that clicking on an iFrame element works with a showcast in a scrolled iFrame.
     */
    @Test
    @MediumTest
    public void testShowCastOnIFrameElementInScrollIFrame() throws Exception {
        ElementReferenceProto element = (ElementReferenceProto) ElementReferenceProto.newBuilder()
                                                .addSelectors("#iframe")
                                                .addSelectors("#touch_area_3")
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
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "iframe", "touch_area_3"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM. The element should be after a
        // big element forcing the page to scroll.
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_3"), is(true));
        tapElement(mTestRule, "iframe", "touch_area_3");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_3"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "iframe", "touch_area_4");
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_4"), is(true));
    }

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }
}
