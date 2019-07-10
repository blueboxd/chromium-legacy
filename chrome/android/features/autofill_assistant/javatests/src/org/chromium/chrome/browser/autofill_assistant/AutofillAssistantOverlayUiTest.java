// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Rect;
import android.graphics.RectF;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.util.DisplayMetrics;

import org.json.JSONArray;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestTouchUtils;

import java.util.Collections;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the Autofill Assistant overlay.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantOverlayUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    // TODO(crbug.com/806868): Create a more specific test site for overlay testing.
    private static final String TEST_PAGE =
            "/components/test/data/autofill_assistant/autofill_assistant_target_website.html";

    @Before
    public void setUp() throws Exception {
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mTestRule.getActivity().getScrim().disableAnimationForTesting(true);
    }

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    /** Creates a coordinator for use in UI tests. */
    private AssistantOverlayCoordinator createCoordinator(AssistantOverlayModel model)
            throws ExecutionException {
        return runOnUiThreadBlocking(
                () -> new AssistantOverlayCoordinator(mTestRule.getActivity(), model));
    }

    /** Tests assumptions about the initial state of the infobox. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        assertScrimDisplayed(false);
        tapElement("touch_area_one");
        assertThat(checkElementExists("touch_area_one"), is(false));
    }

    /** Tests assumptions about the full overlay. */
    @Test
    @MediumTest
    public void testFullOverlay() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL));
        assertScrimDisplayed(true);
        tapElement("touch_area_one");
        assertThat(checkElementExists("touch_area_one"), is(true));

        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.STATE, AssistantOverlayState.HIDDEN));
        assertScrimDisplayed(false);
        tapElement("touch_area_one");
        assertThat(checkElementExists("touch_area_one"), is(false));
    }

    /** Tests assumptions about the partial overlay. */
    @Test
    @MediumTest
    public void testPartialOverlay() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        // Partial overlay, no touchable areas: equivalent to full overlay.
        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.STATE, AssistantOverlayState.PARTIAL));
        assertScrimDisplayed(true);
        tapElement("touch_area_one");
        assertThat(checkElementExists("touch_area_one"), is(true));

        Rect rect = getBoundingRectForElement("touch_area_one");
        runOnUiThreadBlocking(()
                                      -> model.set(AssistantOverlayModel.TOUCHABLE_AREA,
                                              Collections.singletonList(new RectF(rect))));

        // Touchable area set, but no viewport given: equivalent to full overlay.
        tapElement("touch_area_one");
        assertThat(checkElementExists("touch_area_one"), is(true));

        // Set viewport.
        Rect viewport = getViewport();
        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.VISUAL_VIEWPORT, new RectF(viewport)));

        // Now the partial overlay allows tapping the highlighted touch area.
        tapElement("touch_area_one");
        assertThat(checkElementExists("touch_area_one"), is(false));

        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.TOUCHABLE_AREA, Collections.emptyList()));
        tapElement("touch_area_three");
        assertThat(checkElementExists("touch_area_three"), is(true));
    }

    /** Scrolls a touchable area into view and then taps it. */
    @Test
    @MediumTest
    public void testSimpleScrollPartialOverlay() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        Rect rect = getBoundingRectForElement("touch_area_two");
        Rect viewport = getViewport();
        runOnUiThreadBlocking(() -> {
            model.set(AssistantOverlayModel.STATE, AssistantOverlayState.PARTIAL);
            model.set(AssistantOverlayModel.TOUCHABLE_AREA,
                    Collections.singletonList(new RectF(rect)));
            model.set(AssistantOverlayModel.VISUAL_VIEWPORT, new RectF(viewport));
        });
        scrollIntoViewIfNeeded("touch_area_two");
        Rect newViewport = getViewport();
        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.VISUAL_VIEWPORT, new RectF(newViewport)));
        tapElement("touch_area_two");
        assertThat(checkElementExists("touch_area_two"), is(false));
    }

    private void assertScrimDisplayed(boolean expected) throws Exception {
        // Wait for UI thread to be idle.
        onView(isRoot()).check(matches(isDisplayed()));

        // The scrim view is only attached to the view hierarchy when needed, preventing us from
        // using regular espresso facilities.
        boolean scrimInHierarchy =
                runOnUiThreadBlocking(() -> mTestRule.getActivity().getScrim().getParent() != null);
        if (expected && !scrimInHierarchy) {
            throw new Exception("Expected scrim view visible, but scrim was not in view hierarchy");
        }
        if (scrimInHierarchy) {
            if (expected) {
                onView(is(mTestRule.getActivity().getScrim())).check(matches(isDisplayed()));
            } else {
                onView(is(mTestRule.getActivity().getScrim())).check(matches(not(isDisplayed())));
            }
        }
    }

    /** Performs a single tap on the center of the specified element. */
    private void tapElement(String elementId) throws Exception {
        Rect coords = getAbsoluteBoundingRect(elementId);
        float x = coords.left + 0.5f * (coords.right - coords.left);
        float y = coords.top + 0.5f * (coords.bottom - coords.top);

        // Sanity check, can only click on coordinates on screen.
        DisplayMetrics displayMetrics = mTestRule.getActivity().getResources().getDisplayMetrics();
        if (x < 0 || x > displayMetrics.widthPixels || y < 0 || y > displayMetrics.heightPixels) {
            throw new IllegalArgumentException(elementId + " not on screen: tried to tap x=" + x
                    + ", y=" + y + ", which is outside of display with w="
                    + displayMetrics.widthPixels + ", h=" + displayMetrics.heightPixels);
        }
        TestTouchUtils.singleClick(InstrumentationRegistry.getInstrumentation(), x, y);
    }

    /** Computes the bounding rectangle of the specified DOM element in absolute screen space. */
    private Rect getAbsoluteBoundingRect(String elementId) throws Exception {
        // Get bounding rectangle in viewport space.
        Rect elementRect = getBoundingRectForElement(elementId);

        /*
         * Conversion from viewport space to screen space is done in two steps:
         * - First, convert viewport to compositor space (scrolling offset, multiply with factor).
         * - Then, convert compositor space to screen space (add content offset).
         */
        Rect viewport = getViewport();
        float cssToPysicalPixels =
                (((float) mTestRule.getActivity().getCompositorViewHolder().getWidth()
                        / (float) viewport.width()));

        int[] compositorLocation = new int[2];
        mTestRule.getActivity().getCompositorViewHolder().getLocationOnScreen(compositorLocation);
        int offsetY = compositorLocation[1]
                + mTestRule.getActivity().getFullscreenManager().getContentOffset();
        return new Rect((int) ((elementRect.left - viewport.left) * cssToPysicalPixels),
                (int) ((elementRect.top - viewport.top) * cssToPysicalPixels + offsetY),
                (int) ((elementRect.right - viewport.left) * cssToPysicalPixels),
                (int) ((elementRect.bottom - viewport.top) * cssToPysicalPixels + offsetY));
    }

    /**
     * Retrieves the bounding rectangle for the specified element in the DOM tree in CSS pixel
     * coordinates.
     */
    private Rect getBoundingRectForElement(String elementId) throws Exception {
        if (!checkElementExists(elementId)) {
            throw new IllegalArgumentException(elementId + " does not exist");
        }
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(getWebContents(),
                "(function() {"
                        + " rect = document.getElementById('" + elementId
                        + "').getBoundingClientRect();"
                        + " return [window.scrollX + rect.left, window.scrollY + rect.top, "
                        + "         window.scrollX + rect.right, window.scrollY + rect.bottom];"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray rectJson = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return new Rect(
                rectJson.getInt(0), rectJson.getInt(1), rectJson.getInt(2), rectJson.getInt(3));
    }

    /** Checks whether the specified element exists in the DOM tree. */
    private boolean checkElementExists(String elementId) throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(getWebContents(),
                "(function() {"
                        + " return [document.getElementById('" + elementId + "') != null]; "
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray result = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return result.getBoolean(0);
    }

    /**
     * Retrieves the visual viewport of the webpage in CSS pixel coordinates.
     */
    private Rect getViewport() throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(getWebContents(),
                "(function() {"
                        + " const v = window.visualViewport;"
                        + " return [v.pageLeft, v.pageTop, v.width, v.height]"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray values = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return new Rect(values.getInt(0), values.getInt(1), values.getInt(2), values.getInt(3));
    }

    /**
     * Scrolls to the specified element on the webpage, if necessary.
     */
    private void scrollIntoViewIfNeeded(String elementId) throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(getWebContents(),
                "(function() {" + elementId + ".scrollIntoViewIfNeeded();"
                        + " return true;"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
    }
}
