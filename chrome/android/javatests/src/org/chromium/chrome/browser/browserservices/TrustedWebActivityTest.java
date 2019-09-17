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
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for launching
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity} in Trusted Web Activity Mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityTest {
    // TODO(peconn): Add test for navigating away from the trusted origin.
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();
    @Rule
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private String mTestPage;

    @Before
    public void setUp() throws InterruptedException, ProcessInitException {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
    }

    @Test
    @MediumTest
    public void launchesTwa() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        spoofVerification(PACKAGE_NAME, mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void doesntLaunchTwa_WithoutFlag() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        spoofVerification(PACKAGE_NAME, mTestPage);
        createSession(intent, PACKAGE_NAME);

        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void leavesTwa_VerificationFailure() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    /**
     * Test that for trusted activities if the page provides a theme-color and the toolbar color was
     * specified in the intent that the page theme-color is used for the status bar.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // Customizing status bar color is disallowed for tablets.
    public void testStatusBarColorPrecedence() throws TimeoutException, InterruptedException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        final int intentToolbarColor = Color.GREEN;
        final String pageWithThemeColor = mEmbeddedTestServerRule.getServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        final int pageThemeColor = Color.RED;

        Intent intent = createTrustedWebActivityIntent(pageWithThemeColor);
        spoofVerification(PACKAGE_NAME, mTestPage);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, intentToolbarColor);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Waits for theme-color to change so the test doesn't rely on system timing.
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(pageThemeColor, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return activity.getActivityTab().getWebContents().getThemeColor();
                    }
                }));

        int expectedColor = pageThemeColor;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            expectedColor = ColorUtils.getDarkenedColorForStatusBar(expectedColor);
        }
        assertEquals(expectedColor, activity.getWindow().getStatusBarColor());
    }
}
