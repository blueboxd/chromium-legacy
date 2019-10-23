// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.isTrustedWebActivity;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.support.test.filters.MediumTest;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetError;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for launching
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity} in Trusted Web Activity Mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityTest {
    // TODO(peconn): Add test for navigating away from the trusted origin.
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();
    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(mNativeLibraryTestRule, 0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain = RuleChain.emptyRuleChain()
                                          .around(mCustomTabActivityTestRule)
                                          .around(mNativeLibraryTestRule)
                                          .around(mCertVerifierRule)
                                          .around(mEmbeddedTestServerRule);

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private String mTestPage;

    @Before
    public void setUp() {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
    }

    @Test
    @MediumTest
    public void launchesTwa() throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        launchCustomTabActivity(intent);

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void doesntLaunchTwa_WithoutFlag() throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
        launchCustomTabActivity(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void leavesTwa_VerificationFailure() throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    /**
     * Test that if the page provides a theme-color and the toolbar color was specified in the
     * intent that the page theme-color is used for the status bar.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // Customizing status bar color is disallowed for tablets.
    public void testStatusBarColorHasPageThemeColor() throws ExecutionException, TimeoutException {
        final String pageWithThemeColor = mEmbeddedTestServerRule.getServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");

        Intent intent = createTrustedWebActivityIntent(pageWithThemeColor);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.GREEN);
        launchCustomTabActivity(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        waitForThemeColor(activity, Color.RED);
        assertStatusBarColor(activity, Color.RED);
    }

    /**
     * Test that if the page does not provide a theme-color and the toolbar color was specified in
     * the intent that the toolbar color is used for the status bar.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testStatusBarColorNoPageThemeColor() throws ExecutionException, TimeoutException {
        final String pageWithThemeColor = mEmbeddedTestServerRule.getServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        final String pageWithoutThemeColor =
                mEmbeddedTestServerRule.getServer().getURL("/chrome/test/data/simple.html");

        // Navigate to page with a theme color so that we can later wait for the status bar color to
        // change back to the intent color.
        Intent intent = createTrustedWebActivityIntent(pageWithThemeColor);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.GREEN);
        launchCustomTabActivity(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        waitForThemeColor(activity, Color.RED);
        assertStatusBarColor(activity, Color.RED);

        mCustomTabActivityTestRule.loadUrl(pageWithoutThemeColor);
        waitForThemeColor(activity, Color.GREEN);
        assertStatusBarColor(activity, Color.GREEN);
    }

    /**
     * Test that if the page has a certificate error, that the system default color is used
     * (regardless of whether the page provided a theme-color or a toolbar color was specified
     * in the intent).
     */
    @Test
    @DisabledTest(message = "crbug.com/1016743")
    //@MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testStatusBarColorCertificateError() throws ExecutionException, TimeoutException {
        final String pageWithThemeColor = mEmbeddedTestServerRule.getServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        final String pageWithThemeColorCertError = mEmbeddedTestServerRule.getServer().getURL(
                "/chrome/test/data/android/theme_color_test2.html");

        // Initially don't set certificate error so that we can later wait for the status bar color
        // to change back to the default color.
        Intent intent = createTrustedWebActivityIntent(pageWithThemeColor);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.GREEN);
        launchCustomTabActivity(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        waitForThemeColor(activity, Color.RED);
        assertStatusBarColor(activity, Color.RED);

        mCertVerifierRule.setResult(NetError.ERR_CERT_INVALID);
        ChromeTabUtils.loadUrlOnUiThread(activity.getActivityTab(), pageWithThemeColorCertError);

        int expectedColor = Color.BLACK;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            expectedColor = TestThreadUtils.runOnUiThreadBlocking(() -> {
                return TabThemeColorHelper.getDefaultColor(activity.getActivityTab());
            });
        }

        waitForThemeColor(activity, expectedColor);
        assertStatusBarColor(activity, expectedColor);
    }

    public void launchCustomTabActivity(Intent intent) throws TimeoutException {
        String url = intent.getData().toString();
        spoofVerification(PACKAGE_NAME, url);
        createSession(intent, PACKAGE_NAME);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    /**
     * Waits for the Tab's theme-color to change to the passed-in color.
     */
    public static void waitForThemeColor(CustomTabActivity activity, int expectedColor)
            throws ExecutionException, TimeoutException {
        int themeColor = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return TabThemeColorHelper.getColor(activity.getActivityTab()); });
        if (themeColor == expectedColor) {
            return;
        }
        // Use longer-than-default timeout to give page time to finish loading.
        CallbackHelper callbackHelper = new CallbackHelper();
        activity.getActivityTab().addObserver(new EmptyTabObserver() {
            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                if (color == expectedColor) {
                    callbackHelper.notifyCalled();
                }
            }
        });
        callbackHelper.waitForFirst(10, TimeUnit.SECONDS);
    }

    /**
     * Asserts that the status bar color equals the passed-in color.
     */
    public static void assertStatusBarColor(CustomTabActivity activity, int expectedColor) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            expectedColor = ColorUtils.getDarkenedColorForStatusBar(expectedColor);
        }
        assertEquals(expectedColor, activity.getWindow().getStatusBarColor());
    }

    /**
     * Test that trusted web activities show the toolbar when the page has a certificate error
     * (and origin verification succeeds).
     */
    @Test
    @DisabledTest(message = "crbug.com/1016743")
    //@MediumTest
    public void testToolbarVisibleCertificateError() throws ExecutionException, TimeoutException {
        final String pageWithoutCertError =
                mEmbeddedTestServerRule.getServer().getURL("/chrome/test/data/android/about.html");
        final String pageWithCertError = mEmbeddedTestServerRule.getServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");

        // Initially don't set certificate error so that we can later wait for the toolbar to hide.
        launchCustomTabActivity(createTrustedWebActivityIntent(pageWithoutCertError));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        assertFalse(getCanShowToolbarState(tab));

        mCertVerifierRule.setResult(NetError.ERR_CERT_INVALID);
        ChromeTabUtils.loadUrlOnUiThread(tab, pageWithCertError);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getCanShowToolbarState(tab);
            }
        }, 10000, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    public boolean getCanShowToolbarState(Tab tab) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> TabBrowserControlsState.get(tab).canShow());
    }
}
