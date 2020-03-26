// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.filters.SmallTest;
import android.support.v4.app.Fragment;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests handling of external intents.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ExternalNavigationTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final String INTENT_TO_CHROME_URL =
            "intent://play.google.com/store/apps/details?id=com.facebook.katana/#Intent;scheme=https;action=android.intent.action.VIEW;package=com.android.chrome;end";

    private class IntentInterceptor implements InstrumentationActivity.IntentInterceptor {
        public Intent mLastIntent;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void interceptIntent(
                Fragment fragment, Intent intent, int requestCode, Bundle options) {
            mLastIntent = intent;
            mCallbackHelper.notifyCalled();
        }

        public void waitForIntent() {
            try {
                mCallbackHelper.waitForFirst();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    private IntentInterceptor mIntentInterceptor = new IntentInterceptor();

    @Before
    public void setUp() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        activity.setIntentInterceptor(mIntentInterceptor);
    }

    /**
     * Verifies that for a navigation to a URI that WebLayer can handle internally, there
     * is no external intent triggered.
     */
    @Test
    @SmallTest
    public void testBrowserNavigation() throws Throwable {
        mActivityTestRule.navigateAndWait("about:blank");
        Assert.assertNull(mIntentInterceptor.mLastIntent);
    }

    /**
     * Tests that a direct navigation to an external intent is blocked, resulting in a failed
     * browser navigation.
     */
    @Test
    @SmallTest
    public void testExternalIntentWithNoRedirectBlocked() throws Throwable {
        Tab tab = mActivityTestRule.getActivity().getTab();

        // Note that this navigation will not result in a paint.
        mActivityTestRule.navigateAndWaitForFailure(
                tab, INTENT_TO_CHROME_URL, /*waitForPaint=*/false);

        Assert.assertNull(mIntentInterceptor.mLastIntent);
    }

    /**
     * Tests that a navigation that redirects to an external intent results in the external intent
     * being launched.
     */
    @Test
    @SmallTest
    public void testExternalIntentAfterRedirectLaunched() throws Throwable {
        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + INTENT_TO_CHROME_URL);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });

        mIntentInterceptor.waitForIntent();

        Intent intent = mIntentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals("com.android.chrome", intent.getPackage());
        Assert.assertEquals("android.intent.action.VIEW", intent.getAction());
        Assert.assertEquals("https://play.google.com/store/apps/details?id=com.facebook.katana/",
                intent.getDataString());
    }
}
