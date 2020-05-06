// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * This test class checks cookie leakage between all different
 * pairs of Activity types with a constraint that one of the
 * interacting activity must be either Incognito Tab or Incognito CCT.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoCookieLeakageTest {
    private String mCookiesTestPage;
    private EmbeddedTestServer mTestServer;

    private static final String COOKIES_SETTING_PATH = "/chrome/test/data/android/cookie.html";

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
        mCookiesTestPage = mTestServer.getURL(COOKIES_SETTING_PATH);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
        mTestServer.stopAndDestroyServer();
    }

    private void setCookies(Tab tab) throws TimeoutException {
        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab.getWebContents()); });
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");
        assertCookies(tab, "\"Foo=Bar\"");
    }

    private void assertCookies(Tab tab, String expected) throws TimeoutException {
        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab.getWebContents()); });
        String actual = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "getCookie()");
        if (actual.equalsIgnoreCase("null")) actual = "\"\"";
        assertEquals(expected, actual);
    }

    public static class IsolatedFlowsParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> result = new ArrayList<>();
            result.addAll(new TestParams.RegularToIncognito().getParameters());
            result.addAll(new TestParams.IncognitoToRegular().getParameters());
            return result;
        }
    }

    // TODO(crbug.com/1023759) : Currently, incognito CCTs are not isolated and hence they share
    // the session with other incognito sessions. Once, they are properly isolated we should change
    // the test to expect that cookies are not leaked from/to an incognito CCT session.
    @Test
    @MediumTest
    @UseMethodParameter(TestParams.IncognitoToIncognito.class)
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP,
            message = "TODO(crbug.com/1062357): The test is disabled in "
                    + "Android Kitkat because of incognito CCT flakiness.")
    public void
    testCookiesDoNotLeakFromIncognitoToIncognito(
            String incognitoActivityType1, String incognitoActivityType2) throws TimeoutException {
        ActivityType incognitoActivity1 = ActivityType.valueOf(incognitoActivityType1);
        ActivityType incognitoActivity2 = ActivityType.valueOf(incognitoActivityType2);

        Tab setter_tab = incognitoActivity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);
        setCookies(setter_tab);

        Tab getter_tab = incognitoActivity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);

        String expected = CustomTabIncognitoManager.hasIsolatedProfile() ? "\"\"" : "\"Foo=Bar\"";

        assertCookies(getter_tab, expected);
    }

    // This test cookie does not leak from regular to incognito and from incognito to regular
    // session across different activity types.
    @Test
    @MediumTest
    @UseMethodParameter(IsolatedFlowsParams.class)
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP,
            message = "TODO(crbug.com/1062357): The test is disabled in "
                    + "Android Kitkat because of incognito CCT flakiness.")
    public void
    testCookiesDoNotLeakBetweenRegularAndIncognito(
            String setterActivityType, String getterActivityType) throws TimeoutException {
        ActivityType setterActivity = ActivityType.valueOf(setterActivityType);
        ActivityType getterActivity = ActivityType.valueOf(getterActivityType);

        Tab setter_tab = setterActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);
        setCookies(setter_tab);

        Tab getter_tab = getterActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);

        assertCookies(getter_tab, "\"\"");
    }
}
