// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;

import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.UiRestriction;

/** This class tests the functionality of the {@link BottomSheet}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class BottomSheetTest {
    private static final float VELOCITY_WHEN_MOVING_UP = 1.0f;
    private static final float VELOCITY_WHEN_MOVING_DOWN = -1.0f;

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();
    private BottomSheetTestRule.Observer mObserver;
    private TestBottomSheetContent mSheetContent;

    @Before
    public void setUp() throws Exception {
        BottomSheet.setSmallScreenForTesting(false);
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(() -> {
            mSheetContent = new TestBottomSheetContent(mBottomSheetTestRule.getActivity(),
                    BottomSheetContent.ContentPriority.HIGH, false);
        });
        mSheetContent.setPeekHeight(HeightMode.DISABLED);
        mSheetContent.setHalfHeightRatio(0.5f);
        mSheetContent.setSkipHalfStateScrollingDown(false);
        mObserver = mBottomSheetTestRule.getObserver();
    }

    @Test
    @MediumTest
    public void testCustomPeekRatio() {
        int customToolbarHeight = TestBottomSheetContent.TOOLBAR_HEIGHT + 50;
        mSheetContent.setPeekHeight(customToolbarHeight);

        showContent(mSheetContent, SheetState.PEEK);

        assertEquals("Sheet should be peeking at the custom height.", customToolbarHeight,
                mBottomSheetTestRule.getBottomSheetController().getCurrentOffset());
    }

    @Test
    @MediumTest
    public void testMovingDownFromFullClearsThresholdToReachHalfState() {
        showContent(mSheetContent, SheetState.FULL);

        assertEquals("Sheet should reach half state.", SheetState.HALF,
                simulateScrollTo(0.6f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingDownFromFullDoesntClearThresholdToReachHalfState() {
        showContent(mSheetContent, SheetState.FULL);

        assertEquals("Sheet should remain in full state.", SheetState.FULL,
                simulateScrollTo(0.9f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingUpFromHalfClearsThresholdToReachFullState() {
        showContent(mSheetContent, SheetState.HALF);

        assertEquals("Sheet should reach full state.", SheetState.FULL,
                simulateScrollTo(0.8f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_UP));
    }

    @Test
    @MediumTest
    public void testMovingUpFromHalfDoesntClearThresholdToReachHalfState() {
        showContent(mSheetContent, SheetState.HALF);

        assertEquals("Sheet should remain in half state.", SheetState.HALF,
                simulateScrollTo(0.6f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_UP));
    }

    @Test
    @MediumTest
    public void testMovingDownFromHalfClearsThresholdToReachHiddenState() {
        showContent(mSheetContent, SheetState.HALF);

        assertEquals("Sheet should reach hidden state.", SheetState.HIDDEN,
                simulateScrollTo(0.2f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingDownFromHalfDoesntClearThresholdToReachHiddenState() {
        showContent(mSheetContent, SheetState.HALF);

        assertEquals("Sheet should remain in half state.", SheetState.HALF,
                simulateScrollTo(0.4f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
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
