// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.core.AllOf.allOf;
import static org.hamcrest.core.IsNot.not;

import android.os.SystemClock;
import android.view.View;

import androidx.annotation.IdRes;

import org.junit.Assert;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ScrollDirection;
import org.chromium.content_public.browser.test.util.TouchCommon;

/**
 * A utility class that contains methods generic to both the top toolbar and bottom toolbar,
 * and resource ids of views on the toolbar.
 */
public class ToolbarTestUtils {
    // Res ids of views being tested.
    public static final @IdRes int TOP_TOOLBAR = R.id.toolbar;
    public static final @IdRes int TAB_SWITCHER_TOOLBAR = R.id.tab_switcher_toolbar;
    public static final @IdRes int BOTTOM_TOOLBAR = R.id.bottom_toolbar;

    public static final @IdRes int TOP_TOOLBAR_MENU = R.id.menu_button_wrapper;
    public static final @IdRes int TOP_TOOLBAR_HOME = R.id.home_button;
    public static final @IdRes int TOP_TOOLBAR_TAB_SWITCHER = R.id.tab_switcher_button;

    public static final @IdRes int TAB_SWITCHER_TOOLBAR_MENU = R.id.menu_button_wrapper;
    public static final @IdRes int TAB_SWITCHER_TOOLBAR_NEW_TAB = R.id.new_tab_button;
    public static final @IdRes int TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON =
            R.id.tab_switcher_mode_tab_switcher_button;

    public static final @IdRes int BOTTOM_TOOLBAR_HOME = R.id.bottom_home_button;
    public static final @IdRes int BOTTOM_TOOLBAR_SEARCH = R.id.search_accelerator;
    public static final @IdRes int BOTTOM_TOOLBAR_SHARE = R.id.bottom_share_button;
    public static final @IdRes int BOTTOM_TOOLBAR_NEW_TAB = R.id.bottom_new_tab_button;
    public static final @IdRes int BOTTOM_TOOLBAR_TAB_SWITCHER = R.id.bottom_tab_switcher_button;

    public static void checkToolbarVisibility(@IdRes int toolbarId, boolean isVisible) {
        onView(withId(toolbarId)).check(matches(isVisible ? isDisplayed() : not(isDisplayed())));
    }

    public static void checkToolbarButtonVisibility(
            @IdRes int toolbarId, @IdRes int buttonId, boolean isVisible) {
        // We might have buttons with identical ids on both top toolbar and bottom toolbar,
        // so toolbarId is required in order to get the target view correctly.
        onView(allOf(withId(buttonId), isDescendantOfA(withId(toolbarId))))
                .check(matches(isVisible ? isDisplayed() : not(isDisplayed())));
    }

    /**
     * Performs side swipe on the toolbar.
     * @param activity Activity to perform side swipe on.
     * @param direction Direction to perform swipe in.
     */
    public static void performToolbarSideSwipe(
            ChromeActivity activity, @ScrollDirection int direction) {
        Assert.assertTrue("Unexpected direction for side swipe " + direction,
                direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT);
        final View toolbar = activity.findViewById(R.id.toolbar);
        int[] toolbarPos = new int[2];
        toolbar.getLocationOnScreen(toolbarPos);
        final int width = toolbar.getWidth();
        final int height = toolbar.getHeight();

        final int fromX = toolbarPos[0] + width / 2;
        final int toX = toolbarPos[0] + (direction == ScrollDirection.LEFT ? 0 : width);
        final int y = toolbarPos[1] + height / 2;
        final int stepCount = 10;

        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(activity, fromX, y, downTime);
        TouchCommon.dragTo(activity, fromX, toX, y, y, stepCount, downTime);
        TouchCommon.dragEnd(activity, toX, y, downTime);
    }
}
