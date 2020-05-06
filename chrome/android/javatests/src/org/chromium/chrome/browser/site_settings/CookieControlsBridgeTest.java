// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsFeatureList;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Integration tests for CookieControlsBridge.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ContentSettingsFeatureList.IMPROVED_COOKIE_CONTROLS)
public class CookieControlsBridgeTest {
    private class TestCallbackHandler implements CookieControlsObserver {
        private CallbackHelper mHelper;

        public TestCallbackHandler(CallbackHelper helper) {
            mHelper = helper;
        }

        @Override
        public void onCookieBlockingStatusChanged(
                @CookieControlsStatus int status, @CookieControlsEnforcement int enforcement) {
            mStatus = status;
            mEnforcement = enforcement;
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
    private int mEnforcement;
    private int mBlockedCookies;

    @Before
    public void setUp() throws Exception {
        mCallbackHelper = new CallbackHelper();
        mCallbackHandler = new TestCallbackHandler(mCallbackHelper);
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mStatus = CookieControlsStatus.UNINITIALIZED;
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
        assertEquals(CookieControlsStatus.DISABLED, mStatus);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
        assertEquals(0, mBlockedCookies);
    }

    /**
     * Test two callbacks (one for status enabled, one for blocked cookies count) if cookie controls
     * is on. No cookies trying to be set.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCookieBridgeWith3PCookiesEnabled() throws Exception {
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
        assertEquals(CookieControlsStatus.ENABLED, mStatus);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
        assertEquals(0, mBlockedCookies);
    }

    /**
     * Test blocked cookies count changes when new cookie tries to be set. Only one callback because
     * status remains enabled.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCookieBridgeWithChangingBlockedCookiesCount() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Set CookieControlsMode Pref to On
            PrefServiceBridge.getInstance().setInteger(
                    Pref.COOKIE_CONTROLS_MODE, CookieControlsMode.ON);
            // Block all cookies
            WebsitePreferenceBridge.setCategoryEnabled(
                    Profile.getLastUsedRegularProfile(), ContentSettingsType.COOKIES, false);
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
        assertEquals(CookieControlsStatus.ENABLED, mStatus);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
        assertEquals(0, mBlockedCookies);

        // Try to set a cookie on the page when cookies are blocked.
        currentCallCount = mCallbackHelper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");
        mCallbackHelper.waitForCallback(currentCallCount, 1);
        assertEquals(1, mBlockedCookies);
    }

    /**
     * Test blocked cookies works with CookieControlsMode.INCOGNITO_ONLY.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCookieBridgeWithIncognitoSetting() throws Exception {
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
        assertEquals(CookieControlsStatus.DISABLED, mStatus);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
        assertEquals(0, mBlockedCookies);

        // Make new incognito page now
        Tab incognitoTab = mActivityTestRule.loadUrlInNewTab(url, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieControlsBridge =
                    new CookieControlsBridge(mCallbackHandler, incognitoTab.getWebContents());
        });
        mCallbackHelper.waitForCallback(currentCallCount, 2);
        assertEquals(CookieControlsStatus.ENABLED, mStatus);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
        assertEquals(0, mBlockedCookies);
    }
}
