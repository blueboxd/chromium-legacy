// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.widget.ScrimView.ScrimObserver;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;

/**
 * This class tests the behavior of the scrim with the various components that interact with it. The
 * two primary uses are letting the scrim animate manually (as it is used with with omnibox) and
 * manually changing the scrim's alpha (as the bottom sheet uses it).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class ScrimTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mSheetController;
    private ScrimView mScrim;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController = activity.getBottomSheetController();
            mScrim = activity.getScrim();
        });
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testScrimVisibility() throws TimeoutException {
        CallbackHelper visibilityHelper = new CallbackHelper();
        ScrimObserver observer = new ScrimObserver() {
            @Override
            public void onScrimClick() {}

            @Override
            public void onScrimVisibilityChanged(boolean visible) {
                visibilityHelper.notifyCalled();
            }
        };

        final ScrimParams params =
                new ScrimParams(mActivityTestRule.getActivity().getCompositorViewHolder(), true,
                        false, 0, observer);

        int callCount = visibilityHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mScrim.showScrim(params);
            // Skip the animation and set the scrim opacity to 50%.
            mScrim.setViewAlpha(0.5f);
        });
        visibilityHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(true);

        callCount = visibilityHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mScrim.hideScrim(false));
        visibilityHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(false);
    }

    /**
     * Assert that the scrim is the desired visibility.
     * @param visible Whether the scrim should be visible.
     */
    private void assertScrimVisibility(final boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (visible) {
                assertEquals("The scrim should be visible.", View.VISIBLE, mScrim.getVisibility());
            } else {
                assertEquals("The scrim should be invisible.", View.GONE, mScrim.getVisibility());
            }
        });
    }

    /**
     * Wait for the visibility of the scrim to change.
     * @param visible Whether the scrim should be visible.
     */
    private void waitForScrimVisibilityChange(boolean visible) {
        CriteriaHelper.pollUiThread(() -> {
            return (!visible && mScrim.getVisibility() != View.VISIBLE)
                    || (visible && mScrim.getVisibility() == View.VISIBLE);
        });
    }
}
