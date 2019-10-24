// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.browser.ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE;

import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;
import android.view.ViewGroup;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Junit4 rule for tests testing the bottom sheet. This rule creates a new, separate bottom sheet
 * to test with.
 */
@CommandLineFlags.Add({DISABLE_FIRST_RUN_EXPERIENCE})
public class BottomSheetTestRule extends ChromeTabbedActivityTestRule {
    /** An observer used to record events that occur with respect to the bottom sheet. */
    public static class Observer extends EmptyBottomSheetObserver {
        /** A {@link CallbackHelper} that can wait for the bottom sheet to be closed. */
        public final CallbackHelper mClosedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the bottom sheet to be opened. */
        public final CallbackHelper mOpenedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onOffsetChanged event. */
        public final CallbackHelper mOffsetChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onSheetContentChanged event. */
        public final CallbackHelper mContentChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be in its full state. */
        public final CallbackHelper mFullCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be hidden. */
        public final CallbackHelper mHiddenCallbackHelper = new CallbackHelper();

        /** The last value that the onOffsetChanged event sent. */
        private float mLastOffsetChangedValue;

        @Override
        public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
            mLastOffsetChangedValue = heightFraction;
            mOffsetChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOpened(@StateChangeReason int reason) {
            mOpenedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            mClosedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetContentChanged(BottomSheetContent newContent) {
            mContentChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetStateChanged(int newState) {
            if (newState == BottomSheet.SheetState.HIDDEN) {
                mHiddenCallbackHelper.notifyCalled();
            } else if (newState == BottomSheet.SheetState.FULL) {
                mFullCallbackHelper.notifyCalled();
            }
        }

        /** @return The last value passed in to {@link #onSheetOffsetChanged(float)}. */
        public float getLastOffsetChangedValue() {
            return mLastOffsetChangedValue;
        }
    }

    /** A bottom sheet to test with. */
    private BottomSheet mBottomSheet;

    /** A handle to the sheet's observer. */
    private Observer mObserver;

    private @BottomSheet.SheetState int mStartingBottomSheetState = BottomSheet.SheetState.FULL;

    protected void afterStartingActivity() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup coordinator = getActivity().findViewById(R.id.coordinator);
            mBottomSheet = getActivity()
                                   .getLayoutInflater()
                                   .inflate(R.layout.bottom_sheet, coordinator)
                                   .findViewById(R.id.bottom_sheet);
            mBottomSheet.init(coordinator, getActivity().getActivityTabProvider(),
                    getActivity().getFullscreenManager(), getActivity().getWindow(),
                    getActivity().getWindowAndroid().getKeyboardDelegate());
        });

        mObserver = new Observer();
        getBottomSheet().addObserver(mObserver);

        if (mStartingBottomSheetState == BottomSheet.SheetState.PEEK) return;

        setSheetState(mStartingBottomSheetState, /* animate = */ false);
    }

    public void startMainActivityOnBottomSheet(@BottomSheet.SheetState int startingSheetState) {
        mStartingBottomSheetState = startingSheetState;
        startMainActivityOnBlankPage();
    }

    // TODO (aberent): The Chrome test rules currently bypass ActivityTestRule.launchActivity, hence
    // don't call beforeActivityLaunched and afterActivityLaunched as defined in the
    // ActivityTestRule interface. To work round this override the methods that start activities.
    // See https://crbug.com/726444.
    @Override
    public void startMainActivityOnBlankPage() {
        super.startMainActivityOnBlankPage();
        afterStartingActivity();
    }

    public Observer getObserver() {
        return mObserver;
    }

    public BottomSheet getBottomSheet() {
        return mBottomSheet;
    }

    /**
     * Set the bottom sheet's state on the UI thread.
     *
     * @param state   The state to set the sheet to.
     * @param animate If the sheet should animate to the provided state.
     */
    public void setSheetState(int state, boolean animate) {
        TestThreadUtils.runOnUiThreadBlocking(() -> getBottomSheet().setSheetState(state, animate));
    }

    /**
     * Set the bottom sheet's offset from the bottom of the screen on the UI thread.
     *
     * @param offset The offset from the bottom that the sheet should be.
     */
    public void setSheetOffsetFromBottom(float offset) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getBottomSheet().setSheetOffsetFromBottomForTesting(offset));
    }

    public BottomSheetContent getBottomSheetContent() {
        return getBottomSheet().getCurrentSheetContent();
    }

    /**
     * Wait for an update to start and finish.
     */
    public static void waitForWindowUpdates() {
        final long maxWindowUpdateTimeMs = scaleTimeout(1000);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.waitForWindowUpdate(null, maxWindowUpdateTimeMs);
        device.waitForIdle(maxWindowUpdateTimeMs);
    }
}
