// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.Collections;

/**
 * Tests autofill assistant autostart.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantAutostartTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
    }

    /**
     * Launches autofill assistant with a single autostartable script.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "crbug.com/986026")
    public void testAutostart() throws Exception {
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("example.com/hello")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                Collections.singletonList(
                        ActionProto.newBuilder()
                                .setPrompt(
                                        PromptProto.newBuilder()
                                                .setMessage("Hello World!")
                                                .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                        ChipProto.newBuilder()
                                                                .setType(ChipType.DONE_ACTION)
                                                                .setText("Done"))))
                                .build()));

        // Create test service before starting activity.
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();

        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils
                        .createMinimalCustomTabIntent(InstrumentationRegistry.getTargetContext(),
                                "http://www.example.com")
                        .putExtra("org.chromium.chrome.browser.autofill_assistant.ENABLED", true));

        // Wait until autofill assistant is visible on screen.
        CriteriaHelper.pollUiThread(new Criteria("Autofill Assistant never started.") {
            @Override
            public boolean isSatisfied() {
                View view = mTestRule.getActivity().findViewById(R.id.autofill_assistant);
                return view != null && view.getHeight() > 0 && view.isShown();
            }
        });

        onView(withText("Hello World!")).check(matches(isDisplayed()));
    }
}
