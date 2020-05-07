// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheet}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class BottomSheetTest {
    private static final float VELOCITY_WHEN_MOVING_UP = 1.0f;
    private static final float VELOCITY_WHEN_MOVING_DOWN = -1.0f;

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();
    private BottomSheetTestRule.Observer mObserver;
    private TestBottomSheetContent mLowPriorityContent;
    private TestBottomSheetContent mHighPriorityContent;

    @Before
    public void setUp() throws Exception {
        BottomSheet.setSmallScreenForTesting(false);
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(() -> {
            mLowPriorityContent = new TestBottomSheetContent(mBottomSheetTestRule.getActivity(),
                    BottomSheetContent.ContentPriority.LOW, false);
            mHighPriorityContent = new TestBottomSheetContent(mBottomSheetTestRule.getActivity(),
                    BottomSheetContent.ContentPriority.HIGH, false);
        });
        mHighPriorityContent.setPeekHeight(HeightMode.DISABLED);
        mHighPriorityContent.setHalfHeightRatio(0.5f);
        mHighPriorityContent.setSkipHalfStateScrollingDown(false);
        mObserver = mBottomSheetTestRule.getObserver();
    }

    @Test
    @MediumTest
    public void testCustomPeekRatio() {
        int customToolbarHeight = TestBottomSheetContent.TOOLBAR_HEIGHT + 50;
        mHighPriorityContent.setPeekHeight(customToolbarHeight);

        showContent(mHighPriorityContent, SheetState.PEEK);

        assertEquals("Sheet should be peeking at the custom height.", customToolbarHeight,
                mBottomSheetTestRule.getBottomSheetController().getCurrentOffset());
    }

    @Test
    @MediumTest
    public void testMovingDownFromFullClearsThresholdToReachHalfState() {
        showContent(mHighPriorityContent, SheetState.FULL);

        assertEquals("Sheet should reach half state.", SheetState.HALF,
                simulateScrollTo(0.6f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingDownFromFullDoesntClearThresholdToReachHalfState() {
        showContent(mHighPriorityContent, SheetState.FULL);

        assertEquals("Sheet should remain in full state.", SheetState.FULL,
                simulateScrollTo(0.9f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingUpFromHalfClearsThresholdToReachFullState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals("Sheet should reach full state.", SheetState.FULL,
                simulateScrollTo(0.8f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_UP));
    }

    @Test
    @MediumTest
    public void testMovingUpFromHalfDoesntClearThresholdToReachHalfState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals("Sheet should remain in half state.", SheetState.HALF,
                simulateScrollTo(0.6f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_UP));
    }

    @Test
    @MediumTest
    public void testMovingDownFromHalfClearsThresholdToReachHiddenState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals("Sheet should reach hidden state.", SheetState.HIDDEN,
                simulateScrollTo(0.2f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingDownFromHalfDoesntClearThresholdToReachHiddenState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals("Sheet should remain in half state.", SheetState.HALF,
                simulateScrollTo(0.4f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testTabObscuringState() throws ExecutionException, TimeoutException {
        CallbackHelper obscuringStateChangedHelper = new CallbackHelper();
        TabObscuringHandler handler = mBottomSheetTestRule.getActivity().getTabObscuringHandler();
        handler.addObserver((isObscured) -> obscuringStateChangedHelper.notifyCalled());
        mHighPriorityContent.setHasCustomScrimLifecycle(false);

        assertFalse("The tab should not yet be obscured.", handler.areAllTabsObscured());

        int callCount = obscuringStateChangedHelper.getCallCount();
        showContent(mHighPriorityContent, SheetState.HALF);
        obscuringStateChangedHelper.waitForCallback("The tab should be obscured.", callCount);
        assertTrue("The tab should be obscured.", handler.areAllTabsObscured());

        callCount = obscuringStateChangedHelper.getCallCount();
        hideSheet();
        obscuringStateChangedHelper.waitForCallback("The tab should not be obscured.", callCount);

        assertFalse("The tab should not be obscured.", handler.areAllTabsObscured());
    }

    @Test
    @MediumTest
    public void testTabObscuringState_customScrim() throws ExecutionException {
        CallbackHelper obscuringStateChangedHelper = new CallbackHelper();
        TabObscuringHandler handler = mBottomSheetTestRule.getActivity().getTabObscuringHandler();
        handler.addObserver((isObscured) -> obscuringStateChangedHelper.notifyCalled());
        mHighPriorityContent.setHasCustomScrimLifecycle(true);

        assertFalse("The tab should not be obscured.", handler.areAllTabsObscured());

        showContent(mHighPriorityContent, SheetState.HALF);
        assertFalse("The tab should still not be obscured.", handler.areAllTabsObscured());

        hideSheet();

        assertEquals("The obscuring handler should not have been called.", 0,
                obscuringStateChangedHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testOmniboxFocusSuppressesSheet() {
        ToolbarManager toolbarManager = mBottomSheetTestRule.getActivity()
                                                .getRootUiCoordinatorForTesting()
                                                .getToolbarManager();
        showContent(mHighPriorityContent, SheetState.HALF);

        runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(true, 0));

        assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                mBottomSheetTestRule.getBottomSheetController().getSheetState());

        runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(false, 0));

        assertNotEquals("The bottom sheet should not be hidden.", SheetState.HIDDEN,
                mBottomSheetTestRule.getBottomSheetController().getSheetState());
    }

    @Test
    @MediumTest
    public void testSuppressionState_unsuppress() {
        showContent(mHighPriorityContent, SheetState.HALF);

        BottomSheetController controller = mBottomSheetTestRule.getBottomSheetController();
        runOnUiThreadBlocking(() -> {
            controller.suppressSheet(StateChangeReason.NONE);
            ((BottomSheet) controller.getBottomSheetViewForTesting()).endAnimations();
        });

        assertEquals("The sheet should be in the hidden state.", SheetState.HIDDEN,
                controller.getSheetState());

        runOnUiThreadBlocking(() -> {
            controller.unsuppressSheet();
            ((BottomSheet) controller.getBottomSheetViewForTesting()).endAnimations();
        });

        assertEquals("The sheet should be restored to the half state.", SheetState.HALF,
                controller.getSheetState());
    }

    @Test
    @MediumTest
    public void testSuppressionState_unsuppressDifferentContent() {
        showContent(mHighPriorityContent, SheetState.HALF);

        BottomSheetController controller = mBottomSheetTestRule.getBottomSheetController();
        runOnUiThreadBlocking(() -> {
            controller.suppressSheet(StateChangeReason.NONE);
            ((BottomSheet) controller.getBottomSheetViewForTesting()).endAnimations();
        });

        assertEquals("The sheet should be in the hidden state.", SheetState.HIDDEN,
                controller.getSheetState());

        runOnUiThreadBlocking(() -> controller.hideContent(mHighPriorityContent, false));

        showContent(mLowPriorityContent, SheetState.PEEK);

        runOnUiThreadBlocking(() -> {
            controller.unsuppressSheet();
            ((BottomSheet) controller.getBottomSheetViewForTesting()).endAnimations();
        });

        assertEquals("The sheet should be restored to the peek state.", SheetState.PEEK,
                controller.getSheetState());
    }

    private void hideSheet() {
        runOnUiThreadBlocking(
                () -> mBottomSheetTestRule.getBottomSheetController().setSheetStateForTesting(
                        SheetState.HIDDEN, false));
    }

    private float getMaxSheetHeightInPx() {
        return mBottomSheetTestRule.getBottomSheet().getSheetContainerHeight();
    }

    private @SheetState int simulateScrollTo(float targetHeightInPx, float yUpwardsVelocity) {
        return mBottomSheetTestRule.getBottomSheetController().forceScrollingForTesting(
                targetHeightInPx, yUpwardsVelocity);
    }

    /** @param content The content to show in the bottom sheet. */
    private void showContent(BottomSheetContent content, @SheetState int targetState) {
        runOnUiThreadBlocking(() -> {
            mBottomSheetTestRule.getBottomSheetController().requestShowContent(content, false);
        });
        mBottomSheetTestRule.getBottomSheetController().setSheetStateForTesting(targetState, false);
        pollUiThread(()
                             -> mBottomSheetTestRule.getBottomSheetController().getSheetState()
                        == targetState);
    }
}
