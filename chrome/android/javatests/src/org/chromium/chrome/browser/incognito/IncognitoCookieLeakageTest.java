// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.ActivityType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * This test class checks cookie leakage between all different
 * pairs of Activity types with a constraint that one of the
 * interacting activity must be either Incognito Tab or Incognito CCT.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
public class IncognitoCookieLeakageTest {
    private String mCookiesTestPage;
    private EmbeddedTestServer mTestServer;

    private ActivityType mActivity1;
    private ActivityType mActivity2;

    private static final String COOKIES_SETTING_PATH = "/chrome/test/data/android/cookie.html";

    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new IncognitoDataTestUtils.TestParams().getParameters();

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mChromeActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    public IncognitoCookieLeakageTest(String activityType1, String activityType2) {
        mActivity1 = IncognitoDataTestUtils.ActivityType.valueOf(activityType1);
        mActivity2 = IncognitoDataTestUtils.ActivityType.valueOf(activityType2);
    }

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { FirstRunStatus.setFirstRunFlowComplete(true); });
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mCookiesTestPage = mTestServer.getURL(COOKIES_SETTING_PATH);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule); });
        IncognitoDataTestUtils.finishActivities();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { FirstRunStatus.setFirstRunFlowComplete(false); });
        mTestServer.stopAndDestroyServer();
    }

    private String getExpectedString() {
        if (CustomTabIncognitoManager.hasIsolatedProfile()) return "\"\"";
        if (mActivity1.incognito == mActivity2.incognito) return "\"Foo=Bar\"";
        return "\"\"";
    }

    private void setCookies(Tab tab) throws TimeoutException {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");
        assertCookies(tab, "\"Foo=Bar\"");
    }

    private void assertCookies(Tab tab, String expected) throws TimeoutException {
        String actual = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "getCookie()");
        if (actual.equalsIgnoreCase("null")) actual = "\"\"";
        assertEquals(expected, actual);
    }

    @Test
    @LargeTest
    @RetryOnFailure
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP,
            message = "TODO(crbug.com/1023759): The test is disabled in Android Kitkat because of "
                    + "incognito cct flakiness.")
    public void
    testCookiesLeak() throws TimeoutException {
        Tab setter_tab = mActivity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);
        setCookies(setter_tab);

        Tab getter_tab = mActivity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);
        assertCookies(getter_tab, getExpectedString());
    }
}
