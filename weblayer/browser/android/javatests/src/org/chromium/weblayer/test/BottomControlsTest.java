// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Build;
import android.os.RemoteException;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.TestWebLayer;
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
    private int mTopControlsHeight;
    private int mBottomControlsHeight;
    private int mInitialVisiblePageHeight;

    /**
     * Returns the visible height of the page as determined by JS. The returned value is in CSS
     * pixels (which are most likely not the same as device pixels).
     */
    private int getVisiblePageHeight() {
        return mActivityTestRule.executeScriptAndExtractInt("window.innerHeight");
    }

    private void waitForBrowserControlsViewToBeVisible(View v) {
        CriteriaHelper.pollUiThread(() -> {
            Assert.assertTrue(v.getHeight() > 0);
            Assert.assertEquals(View.VISIBLE, v.getVisibility());
        });
    }

    // See TestWebLayer.waitForBrowserControlsMetadataState() for details on this.
    private void waitForBrowserControlsMetadataState(
            InstrumentationActivity activity, int top, int bottom) throws Exception {
        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                TestWebLayer.getTestWebLayer(activity.getApplicationContext())
                        .waitForBrowserControlsMetadataState(activity.getBrowser().getActiveTab(),
                                top, bottom, () -> { latch.countDown(); });
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
        latch.timedAwait();
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testBasic() throws Exception {
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        // Poll until the top view becomes visible.
        waitForBrowserControlsViewToBeVisible(activity.getTopContentsContainer());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMaxControlsHeight = mTopControlsHeight =
                    activity.getTopContentsContainer().getHeight();
            Assert.assertTrue(mMaxControlsHeight > 0);
        });

        // Wait for cc to see the top-controls height.
        waitForBrowserControlsMetadataState(activity, mTopControlsHeight, 0);

        int pageHeightWithTopView = getVisiblePageHeight();

        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        waitForBrowserControlsViewToBeVisible(bottomView);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // The amount necessary to scroll is the sum of the two views. This is because the page
            // height is reduced by the sum of these two.
            mBottomControlsHeight = bottomView.getHeight();
            Assert.assertTrue(mBottomControlsHeight > 0);
            mMaxControlsHeight += mBottomControlsHeight;
        });

        // Wait for cc to see the bottom height. This is very important, as scrolling is gated by
        // cc getting the bottom height.
        waitForBrowserControlsMetadataState(activity, mTopControlsHeight, mBottomControlsHeight);

        // Adding a bottom view should change the page height.
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertNotEquals(getVisiblePageHeight(), pageHeightWithTopView));
        int pageHeightWithTopAndBottomViews = getVisiblePageHeight();
        Assert.assertTrue(pageHeightWithTopAndBottomViews < pageHeightWithTopView);

        // Move by the size of the controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mMaxControlsHeight);

        // Moving should hide the bottom View.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.INVISIBLE, () -> bottomView.getVisibility()));
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(true, () -> getVisiblePageHeight() > pageHeightWithTopView));

        // Move so top and bottom-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mMaxControlsHeight);

        waitForBrowserControlsViewToBeVisible(bottomView);
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertEquals(getVisiblePageHeight(), pageHeightWithTopAndBottomViews));
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    @DisabledTest
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
