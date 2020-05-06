// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.ActivityType;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.TestParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * This test class checks various site storage leaks between all
 * different pairs of Activity types with a constraint that one of the
 * interacting activity must be either incognito tab or incognito CCT.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoStorageLeakageTest {
    private String mSiteDataTestPage;
    private EmbeddedTestServer mTestServer;

    private static final String SITE_DATA_HTML_PATH =
            "/content/test/data/browsing_data/site_data.html";

    private static final List<String> sSiteData = Arrays.asList(
            "LocalStorage", "ServiceWorker", "CacheStorage", "IndexedDb", "FileSystem", "WebSql");

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mChromeActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mSiteDataTestPage = mTestServer.getURL(SITE_DATA_HTML_PATH);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @LargeTest
    @UseMethodParameter(TestParams.AllTypesToAllTypes.class)
    public void testSessionStorageDoesNotLeakFromActivityToActivity(
            String activityType1, String activityType2) throws TimeoutException {
        ActivityType activity1 = ActivityType.valueOf(activityType1);
        ActivityType activity2 = ActivityType.valueOf(activityType2);

        Tab tab1 = activity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);
        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab1.getWebContents()); });

        // Sets the session storage in tab1
        assertEquals("true",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        tab1.getWebContents(), "setSessionStorage()"));

        // Checks the sessions storage is set in tab1
        assertEquals("true",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        tab1.getWebContents(), "hasSessionStorage()"));

        Tab tab2 = activity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);
        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab2.getWebContents()); });

        // Checks the session storage in tab2. Session storage set in tab1 should not be accessible.
        // The session storage is per tab basis.
        assertEquals("false",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        tab2.getWebContents(), "hasSessionStorage()"));
    }

    @Test
    @LargeTest
    @UseMethodParameter(TestParams.AllTypesToAllTypes.class)
    public void testStorageDoesNotLeakFromActivityToActivity(
            String activityType1, String activityType2) throws TimeoutException {
        ActivityType activity1 = ActivityType.valueOf(activityType1);
        ActivityType activity2 = ActivityType.valueOf(activityType2);

        Tab tab1 = activity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);

        Tab tab2 = activity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);

        for (String type : sSiteData) {
            String expected = "false";

            // Both activity types are incognito (one of them being CCT) and they share the storage
            // only if incognito CCT doesn't have an isolated profile.
            if (activity1.incognito && activity2.incognito
                    && !CustomTabIncognitoManager.hasIsolatedProfile()) {
                expected = "true";
            }

            // Both activity types are regular and they share storages.
            if (!activity1.incognito && !activity2.incognito) {
                expected = "true";
            }

            CriteriaHelper.pollUiThread(() -> { assertNotNull(tab1.getWebContents()); });
            // Set the storage in tab1
            assertEquals("true",
                    JavaScriptUtils.runJavascriptWithAsyncResult(
                            tab1.getWebContents(), "set" + type + "()"));

            // Checks the storage is set in tab1
            assertEquals("true",
                    JavaScriptUtils.runJavascriptWithAsyncResult(
                            tab1.getWebContents(), "has" + type + "()"));

            CriteriaHelper.pollUiThread(() -> { assertNotNull(tab2.getWebContents()); });
            // Access the storage from tab2
            assertEquals(expected,
                    JavaScriptUtils.runJavascriptWithAsyncResult(
                            tab2.getWebContents(), "has" + type + "()"));
        }
    }
}
