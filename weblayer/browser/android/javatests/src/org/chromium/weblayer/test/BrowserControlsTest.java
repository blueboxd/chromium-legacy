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
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
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
public class BrowserControlsTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // Height of the top-view. Set in setUp().
    private int mTopViewHeight;
    // Height from the page (obtained using getVisiblePageHeight()) with the top-controls.
    private int mPageHeightWithTopView;

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
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                TestWebLayer.getTestWebLayer(activity.getApplicationContext())
                        .waitForBrowserControlsMetadataState(activity.getBrowser().getActiveTab(),
                                top, bottom, () -> { helper.notifyCalled(); });
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
        helper.waitForCallback(0);
    }

    @Before
    public void setUp() throws Throwable {
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        // Poll until the top view becomes visible.
        waitForBrowserControlsViewToBeVisible(activity.getTopContentsContainer());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTopViewHeight = activity.getTopContentsContainer().getHeight();
            Assert.assertTrue(mTopViewHeight > 0);
        });

        // Wait for cc to see the top-controls height.
        waitForBrowserControlsMetadataState(activity, mTopViewHeight, 0);

        mPageHeightWithTopView = getVisiblePageHeight();
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testTopAndBottom() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        waitForBrowserControlsViewToBeVisible(bottomView);

        int bottomViewHeight =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return bottomView.getHeight(); });
        Assert.assertTrue(bottomViewHeight > 0);
        // The amount necessary to scroll is the sum of the two views. This is because the page
        // height is reduced by the sum of these two.
        int maxViewsHeight = mTopViewHeight + bottomViewHeight;

        // Wait for cc to see the bottom height. This is very important, as scrolling is gated by
        // cc getting the bottom height.
        waitForBrowserControlsMetadataState(activity, mTopViewHeight, bottomViewHeight);

        // Adding a bottom view should change the page height.
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertNotEquals(getVisiblePageHeight(), mPageHeightWithTopView));
        int pageHeightWithTopAndBottomViews = getVisiblePageHeight();
        Assert.assertTrue(pageHeightWithTopAndBottomViews < mPageHeightWithTopView);

        // Move by the size of the controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -maxViewsHeight);

        // Moving should hide the bottom View.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.INVISIBLE, () -> bottomView.getVisibility()));
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(true, () -> getVisiblePageHeight() > mPageHeightWithTopView));

        // Move so top and bottom-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, maxViewsHeight);

        waitForBrowserControlsViewToBeVisible(bottomView);
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertEquals(getVisiblePageHeight(), pageHeightWithTopAndBottomViews));
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testBottomOnly() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        // Remove the top-view.
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });

        // Wait for cc to see the top-controls height change.
        waitForBrowserControlsMetadataState(activity, 0, 0);

        int pageHeightWithNoTopView = getVisiblePageHeight();
        Assert.assertNotEquals(pageHeightWithNoTopView, mPageHeightWithTopView);

        // Add in the bottom-view.
        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        waitForBrowserControlsViewToBeVisible(bottomView);
        int bottomViewHeight =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return bottomView.getHeight(); });
        Assert.assertTrue(bottomViewHeight > 0);
        // Wait for cc to see the bottom-controls height change.
        waitForBrowserControlsMetadataState(activity, 0, bottomViewHeight);

        int pageHeightWithBottomView = getVisiblePageHeight();
        Assert.assertNotEquals(pageHeightWithNoTopView, pageHeightWithBottomView);

        // Move by the size of the bottom-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -bottomViewHeight);

        // Moving should hide the bottom-controls View.
        CriteriaHelper.pollUiThread(
                Criteria.equals(View.INVISIBLE, () -> bottomView.getVisibility()));
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(pageHeightWithNoTopView, () -> getVisiblePageHeight()));

        // Move so bottom-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, bottomViewHeight);

        waitForBrowserControlsViewToBeVisible(bottomView);
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertEquals(getVisiblePageHeight(), pageHeightWithBottomView));
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testTopOnly() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View topView = activity.getTopContentsContainer();

        // Move by the size of the top-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);

        // Moving should hide the top-controls and change the page height.
        CriteriaHelper.pollUiThread(Criteria.equals(View.INVISIBLE, () -> topView.getVisibility()));
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertNotEquals(getVisiblePageHeight(), mPageHeightWithTopView));

        // Move so top-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mTopViewHeight);

        // Wait for the page height to match initial height.
        waitForBrowserControlsViewToBeVisible(topView);
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.assertEquals(getVisiblePageHeight(), mPageHeightWithTopView));
    }

    /**
     * Makes sure that the top controls are shown when a js dialog is shown.
     *
     * Regression test for https://crbug.com/1078181.
     *
     * Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
     */
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testAlertShowsTopControls() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.getActivity();

        // Move by the size of the top-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);

        // Wait till top controls are invisible.
        CriteriaHelper.pollUiThread(Criteria.equals(
                View.INVISIBLE, () -> activity.getTopContentsContainer().getVisibility()));

        // Trigger an alert dialog.
        mActivityTestRule.executeScriptSync(
                "window.setTimeout(function() { alert('alert'); }, 1);", false);

        // Top controls are shown.
        CriteriaHelper.pollUiThread(Criteria.equals(
                View.VISIBLE, () -> activity.getTopContentsContainer().getVisibility()));
    }
}
