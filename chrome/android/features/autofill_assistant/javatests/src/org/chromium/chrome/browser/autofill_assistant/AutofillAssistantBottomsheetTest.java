// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.actionWithAssertions;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getAbsoluteBoundingRect;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE_HEADER;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE_HEADER_CAROUSELS;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.RESIZE_LAYOUT_VIEWPORT;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.RESIZE_VISUAL_VIEWPORT;

import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.action.GeneralLocation;
import android.support.test.espresso.action.GeneralSwipeAction;
import android.support.test.espresso.action.Press;
import android.support.test.espresso.action.Swipe;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing;
import org.chromium.chrome.browser.autofill_assistant.proto.DetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FocusElementProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.feed.library.common.functional.Function;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant bottomsheet.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantBottomsheetTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "bottomsheet_behaviour_target_website.html";

    @Before
    public void setUp() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mTestRule.getActivity().getScrim().disableAnimationForTesting(true);
    }

    private AutofillAssistantTestScript makeScript(ViewportResizing resizing, PeekMode peekMode) {
        ArrayList<ActionProto> list = new ArrayList<>();
        // Prompt.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Hello world!")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("Focus element"))))
                         .build());
        // Set viewport resizing and peek mode.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setConfigureBottomSheet(ConfigureBottomSheetProto.newBuilder()
                                                          .setViewportResizing(resizing)
                                                          .setPeekMode(peekMode))
                         .build());
        // Focus on the bottom element.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setFocusElement(FocusElementProto.newBuilder().setElement(
                                 ElementReferenceProto.newBuilder().addSelectors("p.bottom")))
                         .build());
        // ShowDetails.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowDetails(ShowDetailsProto.newBuilder().setDetails(
                                 DetailsProto.newBuilder()
                                         .setTitle("Details title")
                                         .setShowImagePlaceholder(true)))
                         .build());
        // Add "Done" button.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 Choice.newBuilder().setChip(ChipProto.newBuilder()
                                                                     .setType(ChipType.DONE_ACTION)
                                                                     .setText("Done"))))
                         .build());

        return new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Bottomsheet behaviour")))
                        .build(),
                list);
    }

    @Test
    @MediumTest
    @DisabledTest
    public void testNoResize() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Hello world!"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Details title"), isCompletelyDisplayed());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        // Since no resizing of the viewport happens in this mode, the element is partially covered
        // even when the bottomsheet is minimized
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withText("Details title")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest
    public void testResizeLayoutViewport() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Hello world!"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Details title"), isCompletelyDisplayed());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        // Minimizing the bottomsheet should completely uncover the bottom element.
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        checkElementIsCoveredByBottomsheetByAtMost("bottom", 10);
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withText("Details title")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest
    public void testResizeVisualViewport() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_VISUAL_VIEWPORT, HANDLE)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Hello world!"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Details title"), isCompletelyDisplayed());
        // The viewport should be resized so that the bottom element is not covered by the bottom
        // sheet.
        checkElementIsCoveredByBottomsheetByAtMost("bottom", 10);
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        checkElementIsCoveredByBottomsheetByAtMost("bottom", 10);
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withText("Details title")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest
    public void testHandleHeader() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE_HEADER)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Hello world!"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Details title"), isCompletelyDisplayed());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        checkElementIsCoveredByBottomsheetByAtMost("bottom", 10);
        // The header should be visible even when minimized
        onView(withText("Hello world!")).check(matches(isCompletelyDisplayed()));
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withText("Details title")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest
    public void testHandleHeaderCarousels() {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(
                        makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE_HEADER_CAROUSELS)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Hello world!"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Details title"), isCompletelyDisplayed());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        checkElementIsCoveredByBottomsheetByAtMost("bottom", 10);
        // The header should be visible even when minimized
        onView(withText("Hello world!")).check(matches(isCompletelyDisplayed()));
        // The button gets initially hidden while swiping down but should reappear shortly after.
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom");
        onView(withText("Details title")).check(matches(isCompletelyDisplayed()));
    }

    private ViewAction swipeDownToMinimize() {
        return actionWithAssertions(
                new GeneralSwipeAction(Swipe.FAST, GeneralLocation.CENTER, view -> {
                    float[] coordinates = GeneralLocation.CENTER.calculateCoordinates(view);
                    coordinates[1] =
                            view.getContext().getResources().getDisplayMetrics().heightPixels;
                    return coordinates;
                }, Press.FINGER));
    }

    private ViewAction swipeUpToExpand() {
        return actionWithAssertions(
                new GeneralSwipeAction(Swipe.FAST, GeneralLocation.CENTER, view -> {
                    float[] coordinates = GeneralLocation.CENTER.calculateCoordinates(view);
                    coordinates[1] = 0;
                    return coordinates;
                }, Press.FINGER));
    }

    private void checkElementIsCoveredByBottomsheet(String elementId) {
        validateElementsCoverageByBottomsheet(elementId,
                (Integer percCovered)
                        -> percCovered > 0,
                "Time out while waiting for element '" + elementId
                        + "' to become covered by bottomsheet.");
    }

    private void checkElementIsCoveredByBottomsheetByAtMost(String elementId, int maxPercCovered) {
        validateElementsCoverageByBottomsheet(elementId,
                (Integer percCovered)
                        -> percCovered <= maxPercCovered,
                "Time out while waiting for element '" + elementId
                        + "' to become covered by bottomsheet by at most " + maxPercCovered + "%.");
    }

    // TODO: it would be better to merge this method with waitUntilViewMatchesCondition
    /* Check whether the element is covered by the bottomsheet*/
    private void validateElementsCoverageByBottomsheet(
            String elementId, Function<Integer, Boolean> percValidation, String message) {
        CriteriaHelper.pollInstrumentationThread(new Criteria(message) {
            @Override
            public boolean isSatisfied() {
                try {
                    float y = GeneralLocation.TOP_CENTER.calculateCoordinates(
                            mTestRule.getActivity().findViewById(R.id.bottom_sheet))[1];
                    Rect el = getAbsoluteBoundingRect(elementId, mTestRule);
                    int percCovered = (int) ((el.bottom - y) / (el.bottom - el.top) * 100);
                    return percValidation.apply(percCovered);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        });
    }
}
