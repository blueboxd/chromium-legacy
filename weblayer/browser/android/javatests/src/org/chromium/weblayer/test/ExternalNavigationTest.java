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

    private static final String ABOUT_BLANK_URL = "about:blank";
    private static final String INTENT_TO_CHROME_URL =
            "intent://play.google.com/store/apps/details?id=com.facebook.katana/#Intent;scheme=https;action=android.intent.action.VIEW;package=com.android.chrome;end";
    private static final String NON_RESOLVABLE_INTENT_URL = "intent://garbage;end";

    // The test server handles "echo" with a response containing "Echo" :).
    private final String mTestServerSiteUrl = mActivityTestRule.getTestServer().getURL("/echo");
    private final String mIntentToChromeWithFallbackUrl =
            "intent://play.google.com/store/apps/details?id=com.facebook.katana/#Intent;scheme=https;action=android.intent.action.VIEW;package=com.android.chrome;S.browser_fallback_url="
            + android.net.Uri.encode(mTestServerSiteUrl) + ";end";
    private final String mNonResolvableIntentWithFallbackUrl =
            "intent://play.google.com/store/apps/details?id=com.facebook.katana/#Intent;scheme=https;action=android.intent.action.VIEW;package=com.missing.app;S.browser_fallback_url="
            + android.net.Uri.encode(mTestServerSiteUrl) + ";end";

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

    /**
     * Verifies that for a navigation to a URI that WebLayer can handle internally, there
     * is no external intent triggered.
     */
    @Test
    @SmallTest
    public void testBrowserNavigation() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        mActivityTestRule.navigateAndWait(mTestServerSiteUrl);

        Assert.assertNull(intentInterceptor.mLastIntent);
        Assert.assertEquals(mTestServerSiteUrl, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that a direct navigation to an external intent is blocked, resulting in a failed
     * browser navigation.
     */
    @Test
    @SmallTest
    public void testExternalIntentWithNoRedirectBlocked() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab tab = mActivityTestRule.getActivity().getTab();

        // Note that this navigation will not result in a paint.
        mActivityTestRule.navigateAndWaitForFailure(
                tab, INTENT_TO_CHROME_URL, /*waitForPaint=*/false);

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should not have changed.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that a navigation that redirects to an external intent results in the external intent
     * being launched.
     */
    @Test
    @SmallTest
    public void testExternalIntentAfterRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + INTENT_TO_CHROME_URL);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals("com.android.chrome", intent.getPackage());
        Assert.assertEquals("android.intent.action.VIEW", intent.getAction());
        Assert.assertEquals("https://play.google.com/store/apps/details?id=com.facebook.katana/",
                intent.getDataString());
    }

    /**
     * Tests that a navigation that redirects to an external intent with a fallback URL results in
     * the external intent being launched.
     */
    @Test
    @SmallTest
    public void testExternalIntentWithFallbackUrlAfterRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + mIntentToChromeWithFallbackUrl);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals("com.android.chrome", intent.getPackage());
        Assert.assertEquals("android.intent.action.VIEW", intent.getAction());
        Assert.assertEquals("https://play.google.com/store/apps/details?id=com.facebook.katana/",
                intent.getDataString());
    }

    /**
     * Tests that a navigation that redirects to an external intent that can't be handled results in
     * a failed navigation.
     */
    @Test
    @SmallTest
    public void testNonHandledExternalIntentAfterRedirectBlocked() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + NON_RESOLVABLE_INTENT_URL);

        Tab tab = mActivityTestRule.getActivity().getTab();

        // Note that this navigation will not result in a paint.
        NavigationWaiter waiter = new NavigationWaiter(
                NON_RESOLVABLE_INTENT_URL, tab, /*expectFailure=*/true, /*waitForPaint=*/false);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });
        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should not have changed.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that a navigation that redirects to an external intent that can't be handled but has a
     * fallback URL results in a navigation to the fallback URL.
     */
    @Test
    @SmallTest
    public void testNonHandledExternalIntentWithFallbackUrlAfterRedirectGoesToFallbackUrl()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + mNonResolvableIntentWithFallbackUrl);

        Tab tab = mActivityTestRule.getActivity().getTab();

        NavigationWaiter waiter = new NavigationWaiter(
                mTestServerSiteUrl, tab, /*expectFailure=*/false, /*waitForPaint=*/true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });
        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should now be the fallback URL.
        Assert.assertEquals(mTestServerSiteUrl, mActivityTestRule.getCurrentDisplayUrl());
    }
}
