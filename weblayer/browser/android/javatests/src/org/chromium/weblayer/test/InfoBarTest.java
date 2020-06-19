// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.RemoteException;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Test for infobars.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@CommandLineFlags.Add("enable-features=ImmediatelyHideBrowserControlsForTest")
public class InfoBarTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // When comparing two floats for equality via Assert.assertEquals() it is necessary to provide a
    // delta. Use a sufficiently small value to have confidence in the equality check.
    private static final double FLOAT_EQUALITY_DELTA = 0.0001;

    private Tab getActiveTab() {
        return mActivityTestRule.getActivity().getBrowser().getActiveTab();
    }

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(
                mActivityTestRule.getActivity().getApplicationContext());
    }

    private View getInfoBarContainerView() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                return getTestWebLayer().getInfoBarContainerView(getActiveTab());
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
    }

    private void addInfoBarToActiveTab() throws Exception {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                getTestWebLayer().addInfoBar(getActiveTab(), () -> { helper.notifyCalled(); });
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
        helper.waitForFirst();
    }

    @Before
    public void setUp() throws Throwable {
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        BrowserControlsHelper.createAndBlockUntilBrowserControlsInitializedInSetUp(activity);
    }

    @Test
    @SmallTest
    /**
     * Tests that creation of an infobar impacts the state of the infobar container view as
     * expected.
     *
     */
    public void testAddInfoBar() throws Exception {
        View infoBarContainerView = getInfoBarContainerView();
        Assert.assertEquals(infoBarContainerView.getHeight(), 0);

        addInfoBarToActiveTab();

        Assert.assertTrue(infoBarContainerView.getHeight() > 0);
        Assert.assertEquals(View.VISIBLE, infoBarContainerView.getVisibility());
    }

    @Test
    @SmallTest
    /**
     * Tests that infobars respond to scrolling.
     *
     */
    public void testScrolling() throws Exception {
        addInfoBarToActiveTab();

        View infoBarContainerView = getInfoBarContainerView();
        Assert.assertEquals(infoBarContainerView.getTranslationY(), 0.0, /*delta=*/0.001);

        InstrumentationActivity activity = mActivityTestRule.getActivity();
        int infoBarContainerViewHeight = infoBarContainerView.getHeight();
        Assert.assertTrue(infoBarContainerViewHeight > 0);

        // Scroll down and check that infobar container view is translated in response.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -infoBarContainerViewHeight);
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertEquals(infoBarContainerView.getTranslationY(),
                                infoBarContainerViewHeight, FLOAT_EQUALITY_DELTA));

        // Scroll back up and check that infobar container view is translated in response.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, infoBarContainerViewHeight);
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertEquals(
                                infoBarContainerView.getTranslationY(), 0.0, FLOAT_EQUALITY_DELTA));
    }
}
