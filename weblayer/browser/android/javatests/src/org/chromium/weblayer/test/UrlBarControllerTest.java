// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.RemoteException;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Test class to test UrlBarController logic.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class UrlBarControllerTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final String ABOUT_BLANK_URL = "about:blank";
    private static final String NEW_TAB_URL = "new_browser.html";
    private static final String HTTP_SCHEME = "http://";

    // The test server handles "echo" with a response containing "Echo" :).
    private final String mTestServerSiteUrl = mActivityTestRule.getTestServer().getURL("/echo");

    private String getDisplayedUrl() {
        try {
            InstrumentationActivity activity = mActivityTestRule.getActivity();
            TestWebLayer testWebLayer =
                    TestWebLayer.getTestWebLayer(activity.getApplicationContext());
            View urlBarView = activity.getUrlBarView();
            return testWebLayer.getDisplayedUrl(urlBarView);
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Tests that UrlBarView can be instantiated and shown.
     */
    @Test
    @SmallTest
    public void testShowUrlBar() throws RemoteException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that UrlBarView contains an ImageButton and a TextView with the expected text.
     */
    @Test
    @SmallTest
    public void testUrlBarView() throws RemoteException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        View urlBarView = activity.getUrlBarView();

        Assert.assertEquals(getDisplayedUrl(), ABOUT_BLANK_URL);
    }

    /**
     * Tests that UrlBar TextView is updated when the URL navigated to changes.
     */
    @Test
    @SmallTest
    public void testUrlBarTextViewOnNewNavigation() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        mActivityTestRule.navigateAndWait(mTestServerSiteUrl);
        Assert.assertEquals(mTestServerSiteUrl, mActivityTestRule.getCurrentDisplayUrl());

        View urlBarView = (LinearLayout) activity.getUrlBarView();

        // Remove everything but the TLD because these aren't displayed.
        String mExpectedUrlBarViewText = mTestServerSiteUrl.substring(HTTP_SCHEME.length());
        mExpectedUrlBarViewText =
                mExpectedUrlBarViewText.substring(0, mExpectedUrlBarViewText.indexOf("/echo"));

        Assert.assertEquals(getDisplayedUrl(), mExpectedUrlBarViewText);
    }

    /**
     * Tests that UrlBar TextView is updated when the active tab changes.
     */
    @Test
    @SmallTest
    public void testUrlBarTextViewOnNewActiveTab() {
        String url = mActivityTestRule.getTestDataURL(NEW_TAB_URL);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(activity);

        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        Tab firstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = activity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        // This should launch a new tab and navigate to about:blank.
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());

        callback.waitForNewTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(2, activity.getBrowser().getTabs().size());
            Tab secondTab = activity.getBrowser().getActiveTab();
            Assert.assertNotSame(firstTab, secondTab);
        });

        View urlBarView = activity.getUrlBarView();
        Assert.assertEquals(getDisplayedUrl(), ABOUT_BLANK_URL);
    }
}
