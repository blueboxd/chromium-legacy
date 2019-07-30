// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetObserver}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class BottomSheetObserverTest {
    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();
    private BottomSheetTestRule.Observer mObserver;

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBottomSheet(BottomSheet.SheetState.PEEK);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetTestRule.getBottomSheet().showContent(new TestBottomSheetContent(
                    mBottomSheetTestRule.getActivity(), BottomSheet.ContentPriority.HIGH, false));
        });
        mObserver = mBottomSheetTestRule.getObserver();
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed without animation.
     */
    @Test
    @MediumTest
    public void testCloseEventCalledNoAnimation() throws InterruptedException, TimeoutException {
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.FULL, false);

        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;

        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        int closedCallbackCount = closedCallbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.PEEK, false);
        closedCallbackHelper.waitForCallback(closedCallbackCount, 1);

        assertEquals(initialOpenedCount, mObserver.mOpenedCallbackHelper.getCallCount());
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed with animation.
     */
    @Test
    @MediumTest
    public void testCloseEventCalledWithAnimation() throws InterruptedException, TimeoutException {
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.FULL, false);

        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;

        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        int closedCallbackCount = closedCallbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.PEEK, true);
        closedCallbackHelper.waitForCallback(closedCallbackCount, 1);

        assertEquals(initialOpenedCount, mObserver.mOpenedCallbackHelper.getCallCount());
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened without animation.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalledNoAnimation() throws InterruptedException, TimeoutException {
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.PEEK, false);

        CallbackHelper openedCallbackHelper = mObserver.mOpenedCallbackHelper;

        int initialClosedCount = mObserver.mClosedCallbackHelper.getCallCount();

        int openedCallbackCount = openedCallbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.FULL, false);
        openedCallbackHelper.waitForCallback(openedCallbackCount, 1);

        assertEquals(initialClosedCount, mObserver.mClosedCallbackHelper.getCallCount());
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened with animation.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalledWithAnimation() throws InterruptedException, TimeoutException {
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.PEEK, false);

        CallbackHelper openedCallbackHelper = mObserver.mOpenedCallbackHelper;

        int initialClosedCount = mObserver.mClosedCallbackHelper.getCallCount();

        int openedCallbackCount = openedCallbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.FULL, true);
        openedCallbackHelper.waitForCallback(openedCallbackCount, 1);

        assertEquals(initialClosedCount, mObserver.mClosedCallbackHelper.getCallCount());
    }

    /**
     * Test the onOffsetChanged event.
     */
    @Test
    @MediumTest
    public void testOffsetChangedEvent() throws InterruptedException, TimeoutException {
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.FULL, false);
        CallbackHelper callbackHelper = mObserver.mOffsetChangedCallbackHelper;

        BottomSheet bottomSheet = mBottomSheetTestRule.getBottomSheet();
        float hiddenHeight = bottomSheet.getHiddenRatio() * bottomSheet.getSheetContainerHeight();
        float fullHeight = bottomSheet.getFullRatio() * bottomSheet.getSheetContainerHeight();

        // The sheet's half state is not necessarily 50% of the way to the top.
        float midPeekFull = (hiddenHeight + fullHeight) / 2f;

        // When in the hidden state, the transition value should be 0.
        int callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(hiddenHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // When in the full state, the transition value should be 1.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(fullHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // Halfway between peek and full should send 0.5.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(midPeekFull);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);
    }

    /**
     * Test the onTransitionPeekToHalf event.
     */
    @Test
    @MediumTest
    public void testPeekToHalfTransition() throws InterruptedException, TimeoutException {
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.FULL, false);
        CallbackHelper callbackHelper = mObserver.mPeekToHalfCallbackHelper;

        BottomSheet bottomSheet = mBottomSheetTestRule.getBottomSheet();
        float peekHeight = bottomSheet.getPeekRatio() * bottomSheet.getSheetContainerHeight();
        float halfHeight = bottomSheet.getHalfRatio() * bottomSheet.getSheetContainerHeight();
        float fullHeight = bottomSheet.getFullRatio() * bottomSheet.getSheetContainerHeight();

        float midPeekHalf = (peekHeight + halfHeight) / 2f;
        float midHalfFull = (halfHeight + fullHeight) / 2f;

        // When in the peeking state, the transition value should be 0.
        int callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(peekHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // When in between peek and half states, the transition value should be 0.5.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(midPeekHalf);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // After jumping to the full state (skipping the half state), the event should have
        // triggered once more with a max value of 1.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(fullHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // Moving from full to somewhere between half and full should not trigger the event.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(midHalfFull);
        assertEquals(callbackCount, callbackHelper.getCallCount());

        // Reset the sheet to be between peek and half states.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(midPeekHalf);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // At the half state the event should send 1.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(halfHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);
    }

    @Test
    @MediumTest
    public void testWrapContentBehavior() throws TimeoutException, InterruptedException {
        // We make sure the height of the wrapped content is smaller than sheetContainerHeight.
        BottomSheet bottomSheet = mBottomSheetTestRule.getBottomSheet();
        int wrappedContentHeight = (int) bottomSheet.getSheetContainerHeight() / 2;
        assertTrue(wrappedContentHeight > 0);

        // Show content that should be wrapped.
        CallbackHelper callbackHelper = mObserver.mContentChangedCallbackHelper;
        int callCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheet.showContent(new TestBottomSheetContent(
                    mBottomSheetTestRule.getActivity(), BottomSheet.ContentPriority.HIGH, false) {
                private final ViewGroup mContentView;

                {
                    // We wrap the View in a FrameLayout as we need something to read the hard coded
                    // height in the layout params. There is no way to create a View with a specific
                    // height on its own as View::onMeasure will by default set its height/width to
                    // be the minimum height/width of its background (if any) or expand as much as
                    // it can.
                    mContentView = new FrameLayout(mBottomSheetTestRule.getActivity());
                    View child = new View(mBottomSheetTestRule.getActivity());
                    child.setLayoutParams(new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, wrappedContentHeight));
                    mContentView.addView(child);
                }

                @Override
                public View getContentView() {
                    return mContentView;
                }

                @Override
                public boolean wrapContentEnabled() {
                    return true;
                }
            });
        });
        callbackHelper.waitForCallback(callCount);

        // HALF state is forbidden when wrapping the content.
        mBottomSheetTestRule.setSheetState(BottomSheet.SheetState.HALF, false);
        assertEquals(BottomSheet.SheetState.FULL, bottomSheet.getSheetState());

        // Check the offset.
        assertEquals(wrappedContentHeight + bottomSheet.getToolbarShadowHeight(),
                bottomSheet.getCurrentOffsetPx(), MathUtils.EPSILON);
    }
}
