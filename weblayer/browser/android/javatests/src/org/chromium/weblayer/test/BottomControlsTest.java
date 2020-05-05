// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Build;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Test for bottom-controls.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@CommandLineFlags.Add("enable-features=ImmediatelyHideBrowserControlsForTest")
public class BottomControlsTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private int mMaxControlsHeight;
    private int mInitialVisiblePageHeight;

    /**
     * Returns the visible height of the page as determined by JS. The returned value is in CSS
     * pixels (which are most likely not the same as device pixels).
     */
    private int getVisiblePageHeight() {
        return mActivityTestRule.executeScriptAndExtractInt("window.innerHeight");
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testBasic() throws Exception {
        final String url = UrlUtils.encodeHtmlDataUri("<body><p style='height:5000px'>");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        // Poll until the top view becomes visible.
        CriteriaHelper.pollUiThread(Criteria.equals(
                View.VISIBLE, () -> activity.getTopContentsContainer().getVisibility()));

        // Ask for the page height. While the height isn't needed, the call to the renderer helps
        // ensure the renderer's height has updated based on the top-control.
        getVisiblePageHeight();

        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        // Poll until the bottom view becomes visible.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.VISIBLE, () -> bottomView.getVisibility()));

        // Ask for the page height. While the height isn't needed, the call to the renderer helps
        // ensure the renderer's height has updated based on the top-control.
        getVisiblePageHeight();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Scrolling code ends up using the max height.
            mMaxControlsHeight = Math.max(
                    activity.getTopContentsContainer().getHeight(), bottomView.getHeight());
            Assert.assertTrue(mMaxControlsHeight > 0);
            Assert.assertTrue(bottomView.getHeight() > 0);
        });

        // Move by the size of the controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mMaxControlsHeight);

        // Moving should hide the bottom-controls View.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.INVISIBLE, () -> bottomView.getVisibility()));

        // Move so bottom-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mMaxControlsHeight);

        // bottom-controls are shown async.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.VISIBLE, () -> bottomView.getVisibility()));
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testNoTopControl() throws Exception {
        final String url = UrlUtils.encodeHtmlDataUri("<body><p style='height:5000px'>");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        // Poll until the top view becomes visible.
        CriteriaHelper.pollUiThread(Criteria.equals(
                View.VISIBLE, () -> activity.getTopContentsContainer().getVisibility()));

        // Get the size of the page.
        mInitialVisiblePageHeight = getVisiblePageHeight();
        Assert.assertTrue(mInitialVisiblePageHeight > 0);

        // Swap out the top-view.
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });

        // Wait for the size of the page to change. Don't attempt to correlate the size as the
        // page doesn't see pixels, and to attempt to compare may result in rounding errors. Poll
        // for this value as there is no good way to detect when done.
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertNotEquals(mInitialVisiblePageHeight, getVisiblePageHeight()));

        // Reset mInitialVisiblePageHeight as the top-view is no longer present.
        mInitialVisiblePageHeight = getVisiblePageHeight();

        // Add in the bottom-view.
        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        // Poll until the bottom view becomes visible.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.VISIBLE, () -> bottomView.getVisibility()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMaxControlsHeight = bottomView.getHeight();
            Assert.assertTrue(mMaxControlsHeight > 0);
        });

        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertNotEquals(mInitialVisiblePageHeight, getVisiblePageHeight()));

        // Move by the size of the bottom-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mMaxControlsHeight);

        // Moving should hide the bottom-controls View.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.INVISIBLE, () -> bottomView.getVisibility()));

        // Moving should change the size of the page. Don't attempt to correlate the size as the
        // page doesn't see pixels, and to attempt to compare may result in rounding errors. Poll
        // for this value as there is no good way to detect when done.
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(mInitialVisiblePageHeight, this::getVisiblePageHeight));

        // Move so bottom-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mMaxControlsHeight);

        // bottom-controls are shown async.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.VISIBLE, () -> bottomView.getVisibility()));

        // Wait for the page height to match initial height.
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertNotEquals(mInitialVisiblePageHeight, getVisiblePageHeight()));
    }
}
