// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.website;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Integration tests for CookieControlsBridge.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.IMPROVED_COOKIE_CONTROLS)
public class CookieControlsBridgeTest {
    private class TestCallbackHandler implements CookieControlsBridge.CookieControlsView {
        private CallbackHelper mHelper;

        public TestCallbackHandler(CallbackHelper helper) {
            mHelper = helper;
        }

        @Override
        public void onCookieBlockingStatusChanged(@CookieControlsControllerStatus int status) {
            mStatus = status;
            mHelper.notifyCalled();
        }

        @Override
        public void onBlockedCookiesCountChanged(int blockedCookies) {
            mBlockedCookies = blockedCookies;
            mHelper.notifyCalled();
        }
    }

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    private EmbeddedTestServer mTestServer;
    private CallbackHelper mCallbackHelper;
    private TestCallbackHandler mCallbackHandler;
    private CookieControlsBridge mCookieControlsBridge;
    private int mStatus;
    private int mBlockedCookies;

    @Before
    public void setUp() throws Exception {
        mCallbackHelper = new CallbackHelper();
        mCallbackHandler = new TestCallbackHandler(mCallbackHelper);
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mStatus = CookieControlsControllerStatus.UNINITIALIZED;
        mBlockedCookies = -1;
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Test two callbacks (one for status disabled, one for blocked cookies count) if cookie
     * controls is off.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCookieBridgeWithTPCookiesDisabled() throws Exception {
        int expectedCookiesToBlock = 0;
        int expectedStatus = CookieControlsControllerStatus.DISABLED;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Set CookieControlsMode Pref to Off
            PrefServiceBridge.getInstance().setInteger(
                    Pref.COOKIE_CONTROLS_MODE, CookieControlsMode.OFF);
        });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = mActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieControlsBridge =
                    new CookieControlsBridge(mCallbackHandler, tab.getWebContents());
        });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        Assert.assertEquals(expectedStatus, mStatus);
        Assert.assertEquals(expectedCookiesToBlock, mBlockedCookies);
    }

    /**
     * Test two callbacks (one for status enabled, one for blocked cookies count) if cookie controls
     * is on. No cookies trying to be set.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCookieBridgeWith3PCookiesEnabled() throws Exception {
        int expectedCookiesToBlock = 0;
        int expectedStatus = CookieControlsControllerStatus.ENABLED;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Set CookieControlsMode Pref to On
            PrefServiceBridge.getInstance().setInteger(
                    Pref.COOKIE_CONTROLS_MODE, CookieControlsMode.ON);
        });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = mActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieControlsBridge =
                    new CookieControlsBridge(mCallbackHandler, tab.getWebContents());
        });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        Assert.assertEquals(expectedStatus, mStatus);
        Assert.assertEquals(expectedCookiesToBlock, mBlockedCookies);
    }

    /**
     * Test blocked cookies count changes when new cookie tries to be set. Only one callback because
     * status remains enabled.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCookieBridgeWithChangingBlockedCookiesCount() throws Exception {
        int expectedCookiesToBlock = 0;
        int expectedStatus = CookieControlsControllerStatus.ENABLED;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Set CookieControlsMode Pref to On
            PrefServiceBridge.getInstance().setInteger(
                    Pref.COOKIE_CONTROLS_MODE, CookieControlsMode.ON);
            // Block all cookies
            WebsitePreferenceBridge.setCategoryEnabled(ContentSettingsType.COOKIES, false);
        });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = mActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieControlsBridge =
                    new CookieControlsBridge(mCallbackHandler, tab.getWebContents());
        });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        Assert.assertEquals(expectedStatus, mStatus);
        Assert.assertEquals(expectedCookiesToBlock, mBlockedCookies);

        // Try to set a cookie on the page when cookies are blocked.
        currentCallCount = mCallbackHelper.getCallCount();
        expectedCookiesToBlock = 1;
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");
        mCallbackHelper.waitForCallback(currentCallCount, 1);
        Assert.assertEquals(expectedCookiesToBlock, mBlockedCookies);
    }

    /**
     * Test blocked cookies works with CookieControlsMode.INCOGNITO_ONLY.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCookieBridgeWithIncognitoSetting() throws Exception {
        int expectedCookiesToBlock = 0;
        int expectedStatus = CookieControlsControllerStatus.DISABLED;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Set CookieControlsMode Pref to IncognitoOnly
            PrefServiceBridge.getInstance().setInteger(
                    Pref.COOKIE_CONTROLS_MODE, CookieControlsMode.INCOGNITO_ONLY);
        });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a normal page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = mActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieControlsBridge =
                    new CookieControlsBridge(mCallbackHandler, tab.getWebContents());
        });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        Assert.assertEquals(expectedStatus, mStatus);
        Assert.assertEquals(expectedCookiesToBlock, mBlockedCookies);

        // Make new incognito page now
        expectedStatus = CookieControlsControllerStatus.ENABLED;
        Tab incognitoTab = mActivityTestRule.loadUrlInNewTab(url, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieControlsBridge =
                    new CookieControlsBridge(mCallbackHandler, incognitoTab.getWebContents());
        });
        mCallbackHelper.waitForCallback(currentCallCount, 2);
        Assert.assertEquals(expectedStatus, mStatus);
        Assert.assertEquals(expectedCookiesToBlock, mBlockedCookies);
    }
}
