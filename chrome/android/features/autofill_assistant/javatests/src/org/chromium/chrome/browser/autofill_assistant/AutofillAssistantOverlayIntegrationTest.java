// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
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

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM.
        assertThat(checkElementExists("touch_area_one", mTestRule.getWebContents()), is(true));
        tapElement("touch_area_one", mTestRule);
        assertThat(checkElementExists("touch_area_one", mTestRule.getWebContents()), is(false));
        // Tapping on the element should be blocked by the overlay.
        tapElement("touch_area_four", mTestRule);
        assertThat(checkElementExists("touch_area_four", mTestRule.getWebContents()), is(true));
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

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM. The element should be after a
        // big element forcing the page to scroll.
        assertThat(checkElementExists("touch_area_five", mTestRule.getWebContents()), is(true));
        tapElement("touch_area_five", mTestRule);
        assertThat(checkElementExists("touch_area_five", mTestRule.getWebContents()), is(false));
        // Tapping on the element should be blocked by the overlay.
        tapElement("touch_area_six", mTestRule);
        assertThat(checkElementExists("touch_area_six", mTestRule.getWebContents()), is(true));
    }

    // TODO(b/143942385): Write a test for an element within an iFrame.

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }
}
